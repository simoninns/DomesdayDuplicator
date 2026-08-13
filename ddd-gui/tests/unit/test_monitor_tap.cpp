/************************************************************************

    test_monitor_tap.cpp

    T1 tests for the wait-free publishers the monitoring panels read
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "monitor_tap.h"

namespace ddd::capture {
namespace {

CaptureStats StatsFor(uint64_t index) {
  // Every field derived from one number, so a torn read is detectable: any
  // inconsistency between them means two generations were mixed.
  CaptureStats stats;
  stats.buffers_processed = index;
  stats.transfers_completed = index * 2;
  stats.bytes_written = index * 3;
  stats.samples_written = index * 4;
  stats.slots_in_use = static_cast<size_t>(index % 16);
  stats.metrics.sample_count = index * 5;
  stats.metrics.maximum_value = static_cast<uint16_t>(index % 1024);
  return stats;
}

bool StatsAreConsistent(const CaptureStats& stats) {
  const uint64_t index = stats.buffers_processed;
  return stats.transfers_completed == index * 2 &&
         stats.bytes_written == index * 3 &&
         stats.samples_written == index * 4 &&
         stats.slots_in_use == static_cast<size_t>(index % 16) &&
         stats.metrics.sample_count == index * 5 &&
         stats.metrics.maximum_value == static_cast<uint16_t>(index % 1024);
}

TEST(StatsPublisherTest, AReaderSeesWhatWasPublished) {
  StatsPublisher publisher;
  publisher.Publish(StatsFor(42));

  const CaptureStats read = publisher.Read();
  EXPECT_EQ(read.buffers_processed, 42U);
  EXPECT_TRUE(StatsAreConsistent(read));
}

TEST(StatsPublisherTest, TheGenerationCountsPublications) {
  StatsPublisher publisher;
  EXPECT_EQ(publisher.Generation(), 0U);

  publisher.Publish(StatsFor(1));
  publisher.Publish(StatsFor(2));
  EXPECT_EQ(publisher.Generation(), 2U);
}

TEST(StatsPublisherTest, AReaderNeverSeesTwoGenerationsMixedTogether) {
  // The failure this guards against is subtle and would look like a hardware
  // fault: a panel showing a sample count from one buffer beside a clip count
  // from another, and a user concluding the device is misbehaving.
  StatsPublisher publisher;
  std::atomic<bool> stop{false};
  std::atomic<uint64_t> torn{0};
  std::atomic<uint64_t> reads{0};

  std::thread reader([&] {
    while (!stop.load()) {
      const CaptureStats stats = publisher.Read();
      if (!StatsAreConsistent(stats)) {
        ++torn;
      }
      ++reads;
    }
  });

  for (uint64_t index = 1; index <= 200'000; ++index) {
    publisher.Publish(StatsFor(index));
  }

  stop = true;
  reader.join();

  EXPECT_EQ(torn.load(), 0U);
  EXPECT_GT(reads.load(), 0U) << "the reader never ran, so nothing was tested";
}

TEST(SnapshotPublisherTest, NothingIsAvailableUntilSomethingIsPublished) {
  SnapshotPublisher publisher(64);

  std::vector<uint8_t> out;
  uint64_t generation = 0;
  EXPECT_FALSE(publisher.TryRead(out, generation));
}

TEST(SnapshotPublisherTest, ASnapshotArrivesIntactAndOnlyOnce) {
  SnapshotPublisher publisher(64);

  std::vector<uint8_t> source(64);
  for (size_t index = 0; index < source.size(); ++index) {
    source[index] = static_cast<uint8_t>(index);
  }
  publisher.Publish(source.data(), source.size());

  std::vector<uint8_t> out;
  uint64_t generation = 0;
  ASSERT_TRUE(publisher.TryRead(out, generation));
  EXPECT_EQ(out, source);
  EXPECT_EQ(generation, 1U);

  // A display refreshing faster than snapshots are published is the ordinary
  // case, and it must not redraw the same data as if it were new.
  EXPECT_FALSE(publisher.TryRead(out, generation));
}

TEST(SnapshotPublisherTest, ABufferLargerThanTheSnapshotIsTruncatedNotRefused) {
  // The processing thread hands over a 2 MB buffer; the snapshot is 64 KiB.
  // Taking the front of it is the whole intent.
  SnapshotPublisher publisher(16);

  std::vector<uint8_t> source(1024, 0xA5);
  publisher.Publish(source.data(), source.size());

  std::vector<uint8_t> out;
  uint64_t generation = 0;
  ASSERT_TRUE(publisher.TryRead(out, generation));
  EXPECT_EQ(out.size(), 16U);
}

TEST(SnapshotPublisherTest,
     ASlowReaderDropsSnapshotsRatherThanHoldingUpTheWriter) {
  // Falling behind is correct behaviour, not a fault: an old snapshot of a live
  // signal is of no interest, and the alternative — making the processing
  // thread wait — would cost samples.
  SnapshotPublisher publisher(8);

  std::vector<uint8_t> source(8);
  for (uint8_t index = 1; index <= 5; ++index) {
    std::fill(source.begin(), source.end(), index);
    publisher.Publish(source.data(), source.size());
  }

  std::vector<uint8_t> out;
  uint64_t generation = 0;
  ASSERT_TRUE(publisher.TryRead(out, generation));
  EXPECT_EQ(generation, 5U) << "the reader must get the newest, not the oldest";
  EXPECT_EQ(out.front(), 5);
}

TEST(SnapshotPublisherTest, AHammeringReaderNeverSeesAHalfWrittenSnapshot) {
  // Every snapshot is filled with a single repeated byte, so a buffer caught
  // mid-write would contain two different values — which is precisely what the
  // triple buffering exists to make impossible.
  constexpr size_t kSnapshotBytes = 4096;
  SnapshotPublisher publisher(kSnapshotBytes);

  std::atomic<bool> stop{false};
  std::atomic<uint64_t> torn{0};
  std::atomic<uint64_t> reads{0};

  std::thread reader([&] {
    std::vector<uint8_t> out;
    uint64_t generation = 0;
    while (!stop.load()) {
      if (!publisher.TryRead(out, generation)) {
        continue;
      }
      ++reads;
      for (uint8_t byte : out) {
        if (byte != out.front()) {
          ++torn;
          break;
        }
      }
    }
  });

  std::vector<uint8_t> source(kSnapshotBytes);
  for (uint64_t index = 1; index <= 20'000; ++index) {
    std::fill(source.begin(), source.end(), static_cast<uint8_t>(index));
    publisher.Publish(source.data(), source.size());
  }

  stop = true;
  reader.join();

  EXPECT_EQ(torn.load(), 0U);
  EXPECT_GT(reads.load(), 0U) << "the reader never ran, so nothing was tested";
}

// The property the whole tap design exists for, measured directly rather than
// inferred from throughput.
//
// "Wait-free" means the writer's cost does not depend on what any reader is
// doing. The natural way to test that is to compare pipeline throughput with
// and without a reader — but that measurement is confounded: a reader spinning
// with no pause at all copies the stats block millions of times a second and
// saturates memory bandwidth the pipeline needs, so throughput falls even
// though nothing ever blocks. (Measured on an 8-core Ryzen 5800X: an unbounded
// reader took an unpaced pipeline from 600 MB/s to 140 MB/s, with publish
// latency unchanged.) Aggregate throughput cannot tell contention for a lock
// apart from contention for a memory controller.
//
// The writer's own publish time can. If publication took a lock, a reader
// holding it would show up here immediately, and a descheduled lock-holder
// would show up as a spike of milliseconds.
//
// Readers are not entirely free, and the test says so rather than claiming
// otherwise: four of them pulling the stats block into shared cache state takes
// a publication from around 25 ns to around 190 ns on this machine. That is
// cache-line invalidation, which is unavoidable for any design where a reader
// reads what a writer writes. What matters is that it stays a fixed small cost
// — 190 ns against a 26 ms buffer period is seven millionths of the budget —
// rather than becoming a wait whose length is somebody else's business.
TEST(StatsPublisherTest, PublishingStaysWaitFreeUnderAHammeringReader) {
  constexpr int kPublications = 200'000;
  constexpr int kReaderCount = 4;

  struct PublishCost {
    double mean_nanoseconds = 0.0;
    int64_t worst_nanoseconds = 0;
  };

  const auto measure_publish_cost = [](StatsPublisher& publisher) {
    PublishCost cost;
    int64_t total = 0;

    for (int index = 1; index <= kPublications; ++index) {
      const CaptureStats stats = StatsFor(static_cast<uint64_t>(index));

      const auto started = std::chrono::steady_clock::now();
      publisher.Publish(stats);
      const auto elapsed = std::chrono::steady_clock::now() - started;

      const int64_t nanoseconds =
          std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
      total += nanoseconds;
      cost.worst_nanoseconds = std::max(cost.worst_nanoseconds, nanoseconds);
    }

    cost.mean_nanoseconds =
        static_cast<double>(total) / static_cast<double>(kPublications);
    return cost;
  };

  PublishCost undisturbed;
  {
    StatsPublisher publisher;
    undisturbed = measure_publish_cost(publisher);
  }

  PublishCost contended;
  {
    StatsPublisher publisher;
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> torn{0};

    std::vector<std::thread> readers;
    readers.reserve(kReaderCount);
    for (int index = 0; index < kReaderCount; ++index) {
      readers.emplace_back([&] {
        while (!stop.load()) {
          if (!StatsAreConsistent(publisher.Read())) {
            ++torn;
          }
        }
      });
    }

    contended = measure_publish_cost(publisher);

    stop = true;
    for (std::thread& reader : readers) {
      reader.join();
    }
    EXPECT_EQ(torn.load(), 0U);
  }

  std::cout << "[          ] publish cost: " << undisturbed.mean_nanoseconds
            << " ns mean / " << undisturbed.worst_nanoseconds
            << " ns worst undisturbed, " << contended.mean_nanoseconds
            << " ns mean / " << contended.worst_nanoseconds << " ns worst with "
            << kReaderCount << " readers hammering\n";

  // The mean is the evidence, and the bound is absolute rather than relative to
  // the undisturbed figure. A relative bound would be measuring the wrong
  // thing: the undisturbed cost is a couple of tens of nanoseconds, so ordinary
  // cache effects move the ratio around a lot while moving the absolute cost
  // hardly at all. What would fail here is a lock — a contended one costs
  // microseconds, and one whose holder has been descheduled costs milliseconds.
  //
  // The worst case is printed for information but not asserted on. It is a
  // scheduler preemption landing inside the timed region rather than a property
  // of the code, and a test that asserted on it would fail on a busy machine
  // for reasons that say nothing about the tap.
  EXPECT_LT(contended.mean_nanoseconds, 5'000.0)
      << "publishing became expensive once readers appeared, which is what a "
         "lock on the publish path would look like";
}

}  // namespace
}  // namespace ddd::capture
