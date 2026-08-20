/************************************************************************

    test_signal_watcher.cpp

    T1 tests for turning an interrupt into an event-loop signal
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QMetaObject>
#include <QSignalSpy>
#include <chrono>
#include <thread>

#include "signal_watcher.h"

#ifndef Q_OS_WIN
#include <csignal>
#endif

namespace ddd::gui {
namespace {

using namespace std::chrono_literals;

// Pump the event loop until a condition holds, so a test fails with a message
// rather than hanging. The whole point of this class is that the interrupt is
// delivered later, on the loop, so there is nothing to block on.
template <typename Predicate>
bool PumpUntil(Predicate predicate, std::chrono::milliseconds limit = 2000ms) {
  const auto deadline = std::chrono::steady_clock::now() + limit;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents();
    std::this_thread::sleep_for(1ms);
  }
  return predicate();
}

#ifdef Q_OS_WIN

// What can be tested here and what cannot.
//
// The delivery is testable: the console handler's whole job is to post to the
// watcher from another thread, and a test can post the same way and prove the
// interrupt comes out on the event loop rather than wherever it arrived.
//
// The arrival is not. A console control event is sent to a whole process
// group, and this test runs in the group CTest is running in — raising a real
// Ctrl+C here would interrupt the test runner along with everything beside it.
// That half is covered by the manual pass on Windows.
class SignalWatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    watcher_ = SignalWatcher::Install();
    ASSERT_NE(watcher_, nullptr);
  }

  void TearDown() override {
    delete watcher_;
    watcher_ = nullptr;
  }

  // The one thing the console handler does, from a thread of its own as it
  // does it.
  void PostFromAnotherThread() {
    std::thread([this] {
      QMetaObject::invokeMethod(watcher_, "Deliver", Qt::QueuedConnection);
    }).join();
  }

  SignalWatcher* watcher_ = nullptr;
};

TEST_F(SignalWatcherTest, AnInterruptArrivesOnTheEventLoop) {
  QSignalSpy interrupts(watcher_, &SignalWatcher::Interrupted);

  PostFromAnotherThread();

  // Nothing yet: the point of the posting is that the interrupt waits for the
  // loop rather than running on whatever thread Windows delivered it on.
  EXPECT_EQ(interrupts.count(), 0);

  EXPECT_TRUE(PumpUntil([&interrupts] { return interrupts.count() == 1; }));
  EXPECT_EQ(interrupts.count(), 1);
}

TEST_F(SignalWatcherTest, ThereIsOnlyEverOneOfThem) {
  EXPECT_TRUE(SignalWatcher::installed());
  EXPECT_EQ(SignalWatcher::Install(), nullptr);
}

TEST_F(SignalWatcherTest, DestroyingItUninstallsIt) {
  delete watcher_;
  watcher_ = nullptr;

  EXPECT_FALSE(SignalWatcher::installed());

  SignalWatcher* replacement = SignalWatcher::Install();
  EXPECT_NE(replacement, nullptr);
  delete replacement;
}

#else

class SignalWatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    watcher_ = SignalWatcher::Install();
    ASSERT_NE(watcher_, nullptr);
  }

  // Deleted rather than left to the process, so that the next test starts with
  // the default disposition and a raise() here cannot reach a watcher that
  // belonged to a test that has finished.
  void TearDown() override {
    delete watcher_;
    watcher_ = nullptr;
  }

  SignalWatcher* watcher_ = nullptr;
};

// The assertion that matters. Raising it proves the process was not ended by
// it, which is the default disposition and the thing that would leave a capture
// file unfinished; the spy proves it arrived somewhere the capture can be
// stopped properly.
TEST_F(SignalWatcherTest, AnInterruptArrivesOnTheEventLoop) {
  QSignalSpy interrupts(watcher_, &SignalWatcher::Interrupted);

  ASSERT_EQ(::raise(SIGINT), 0);

  EXPECT_TRUE(PumpUntil([&interrupts] { return interrupts.count() == 1; }));
  EXPECT_EQ(interrupts.count(), 1);
}

// kill(1) sends this one, and it is what a script uses rather than Ctrl+C.
TEST_F(SignalWatcherTest, ATerminationRequestArrivesTheSameWay) {
  QSignalSpy interrupts(watcher_, &SignalWatcher::Interrupted);

  ASSERT_EQ(::raise(SIGTERM), 0);

  EXPECT_TRUE(PumpUntil([&interrupts] { return interrupts.count() == 1; }));
  EXPECT_EQ(interrupts.count(), 1);
}

// A user pressing Ctrl+C twice because the first one looked like it did nothing
// is asking for the same thing twice, not for something else. Both arrive
// before the loop is pumped, and one request comes out.
TEST_F(SignalWatcherTest, TwoInterruptsTogetherAreOneRequest) {
  QSignalSpy interrupts(watcher_, &SignalWatcher::Interrupted);

  ASSERT_EQ(::raise(SIGINT), 0);
  ASSERT_EQ(::raise(SIGINT), 0);

  EXPECT_TRUE(PumpUntil([&interrupts] { return interrupts.count() >= 1; }));

  // Pump a little longer, so a second delivery would have somewhere to land if
  // one were coming.
  PumpUntil([] { return false; }, 50ms);
  EXPECT_EQ(interrupts.count(), 1);
}

// The handler reaches one descriptor, held in a file-scope variable because a
// signal handler is handed no context of its own. A second watcher would take
// that descriptor away from the first, so there is no second watcher.
TEST_F(SignalWatcherTest, ThereIsOnlyEverOneOfThem) {
  EXPECT_TRUE(SignalWatcher::installed());
  EXPECT_EQ(SignalWatcher::Install(), nullptr);
}

TEST_F(SignalWatcherTest, DestroyingItUninstallsIt) {
  delete watcher_;
  watcher_ = nullptr;

  EXPECT_FALSE(SignalWatcher::installed());

  // And the seat is free, which is what proves the descriptor and the handler
  // were both let go rather than merely forgotten.
  SignalWatcher* replacement = SignalWatcher::Install();
  EXPECT_NE(replacement, nullptr);
  delete replacement;
}

#endif  // Q_OS_WIN

}  // namespace
}  // namespace ddd::gui
