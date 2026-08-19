/************************************************************************

    test_board_bringup_wizard.cpp

    The bring-up flow as a widget, driven end to end with no hardware
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>
#include <string>
#include <vector>

#include "board_bringup_wizard.h"
#include "boot_image_fixture.h"
#include "fake_device_programmer.h"
#include "fake_device_updater.h"
#include "fake_jtag_cable.h"
#include "svf_fixtures.h"
#include "update_bundle.h"
#include "update_fixtures.h"
#include "update_key.h"
#include "wire_protocol.h"

namespace ddd::gui {
namespace {

using capture::FakeDeviceProgrammer;
using capture::FakeDeviceUpdater;
using capture::FakeJtagCable;

// Every state of a bring-up, including the two that damage hardware if they
// happen in the wrong order, driven here on a machine with nothing plugged in.
// That is not a convenience: a board in its boot ROM, a legacy board, a
// charge-only cable and a power cycle nobody performed cannot all be arranged
// on one bench, and several of them cannot be arranged at all.

// A signed release bundle on disk, carrying all four payloads. Built from the
// same fixtures the engine tests use, because a wizard that only ever met a
// file its own test wrote would meet the real format for the first time on
// somebody's bench.
class BringUpFile {
 public:
  BringUpFile() {
    capture::UstarWriter writer;
    writer.AddFile(capture::kManifestEntryName,
                   capture::test::Bytes(capture::test::kBringUpManifestJson));
    writer.AddFile(
        capture::kSignatureEntryName,
        capture::test::Bytes(capture::test::kBringUpManifestSignature));
    writer.AddFile(capture::kFirmwareEntryName, capture::test::MakeBootImage());
    writer.AddFile(capture::kGatewareEntryName,
                   capture::test::Bytes(capture::test::kGatewarePayload));
    writer.AddFile(capture::kProvisioningEntryName,
                   capture::test::Bytes(capture::test::kProvisioningPayload));
    writer.AddFile(
        capture::kFactoryGatewareEntryName,
        capture::test::Bytes(capture::test::kFactoryGatewarePayload));
    Write(writer.Finish(), QStringLiteral("update.dddfw"));
  }

  // A release bundle from before the bring-up payloads existed: firmware and
  // application gateware only. It updates a working device perfectly well and
  // cannot bring a board up, and it is the mistake somebody is most likely to
  // make here.
  static QString UpdateOnlyPath(QTemporaryDir& directory) {
    capture::UstarWriter writer;
    writer.AddFile(capture::kManifestEntryName,
                   capture::test::Bytes(capture::test::kManifestJson));
    writer.AddFile(capture::kSignatureEntryName,
                   capture::test::Bytes(capture::test::kManifestSignature));
    writer.AddFile(capture::kFirmwareEntryName,
                   capture::test::Bytes(capture::test::kFirmwarePayload));
    writer.AddFile(capture::kGatewareEntryName,
                   capture::test::Bytes(capture::test::kGatewarePayload));

    const std::vector<uint8_t> archive = writer.Finish();
    const QString path =
        directory.filePath(QStringLiteral("update-only.dddfw"));
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
      file.write(reinterpret_cast<const char*>(archive.data()),
                 static_cast<qint64>(archive.size()));
    }
    return path;
  }

  const QString& path() const { return path_; }
  QTemporaryDir& directory() { return directory_; }

 private:
  void Write(const std::vector<uint8_t>& archive, const QString& name) {
    path_ = directory_.filePath(name);
    QFile file(path_);
    if (file.open(QIODevice::WriteOnly)) {
      file.write(reinterpret_cast<const char*>(archive.data()),
                 static_cast<qint64>(archive.size()));
    }
  }

  QTemporaryDir directory_;
  QString path_;
};

// Non-owning forwarders, for the reason the update page's tests have them: the
// wizard hands ownership to its worker, so the fakes themselves cannot be what
// is handed over or a test could not read them afterwards.
class BorrowedProgrammer : public capture::IDeviceProgrammer {
 public:
  explicit BorrowedProgrammer(FakeDeviceProgrammer* fake) : fake_(fake) {}

  bool WriteRam(uint32_t address, std::span<const uint8_t> data) override {
    return fake_->WriteRam(address, data);
  }
  bool Start(uint32_t entry_address) override {
    return fake_->Start(entry_address);
  }
  std::optional<std::string> WaitForApplication(
      std::chrono::milliseconds timeout) override {
    return fake_->WaitForApplication(timeout);
  }

 private:
  FakeDeviceProgrammer* fake_ = nullptr;
};

class BorrowedUpdater : public capture::IDeviceUpdater {
 public:
  explicit BorrowedUpdater(FakeDeviceUpdater* fake) : fake_(fake) {}

  std::optional<capture::DeviceIdentity> ReadIdentity() override {
    return fake_->ReadIdentity();
  }
  std::optional<capture::DeviceUpdateStatus> ReadStatus() override {
    return fake_->ReadStatus();
  }
  bool Begin(capture::UpdateTarget target, uint64_t length,
             const capture::Sha256Digest& digest) override {
    return fake_->Begin(target, length, digest);
  }
  bool SendChunk(capture::UpdateTarget target, uint16_t index,
                 std::span<const uint8_t> data) override {
    return fake_->SendChunk(target, index, data);
  }
  bool Finish(capture::UpdateTarget target) override {
    return fake_->Finish(target);
  }
  bool Reset() override { return fake_->Reset(); }
  bool ReconfigureFpga() override { return fake_->ReconfigureFpga(); }
  std::optional<capture::DeviceIdentity> WaitForReturn(
      std::chrono::milliseconds timeout) override {
    return fake_->WaitForReturn(timeout);
  }

 private:
  FakeDeviceUpdater* fake_ = nullptr;
};

class BorrowedCable : public capture::IJtagCable {
 public:
  explicit BorrowedCable(FakeJtagCable* fake) : fake_(fake) {}

  bool Shift(std::span<const uint8_t> tms, std::span<const uint8_t> tdi,
             size_t bit_count, std::vector<uint8_t>* tdo) override {
    return fake_->Shift(tms, tdi, bit_count, tdo);
  }
  bool RunClock(size_t count) override { return fake_->RunClock(count); }
  bool Flush() override { return fake_->Flush(); }
  const char* Name() const override { return fake_->Name(); }

 private:
  FakeJtagCable* fake_ = nullptr;
};

capture::DeviceInfo Device(capture::DevicePersonality personality) {
  capture::DeviceInfo info;
  info.path = "bus-1-port-2";
  info.personality = personality;
  info.protocol_version =
      personality == capture::DevicePersonality::kApplication ? 1 : 0;
  info.speed = capture::DeviceSpeed::kSuper;
  if (personality == capture::DevicePersonality::kApplication) {
    info.product_string = "Domesday Duplicator (0123abcd)";
  }
  return info;
}

// What a finished board reports: its own firmware, and an FPGA that has loaded
// the *application* image out of its own flash.
//
// The image role is what separates a board that has restarted from one that
// never lost power: the gateware step puts the factory image into the FPGA
// over JTAG, and only a power cycle replaces it with this.
capture::DeviceIdentity BroughtUpIdentity() {
  capture::DeviceIdentity identity;
  identity.product_string = "Domesday Duplicator (0123abcd)";
  identity.protocol_version = 1;
  identity.gateware_present = true;
  identity.register_map_version = capture::kRegisterMapVersionMaximum;
  identity.image_role = capture::kImageRoleApplication;
  return identity;
}

// The bench, as a struct: what is on the bus, whether the cable opens, and the
// fakes behind both.
struct WizardUnderTest {
  std::vector<capture::DeviceInfo> bus;
  capture::UsbPresence cable_presence = capture::UsbPresence::kPresent;
  capture::UsbPresence bridge_presence = capture::UsbPresence::kAbsent;
  bool cable_opens = true;

  FakeDeviceProgrammer programmer;
  FakeDeviceUpdater updater;
  FakeJtagCable cable;

  BringUpFile file;
  std::unique_ptr<BoardBringUpWizard> wizard;

  // What this "build" was packaged with, if anything. Empty is the ordinary
  // state for a build from source, and the state every test that does not say
  // otherwise runs in.
  explicit WizardUnderTest(capture::DevicePersonality personality =
                               capture::DevicePersonality::kRecovery,
                           const QString& bundled = QString()) {
    bus.push_back(Device(personality));

    BoardBringUpWizard::Access access;
    access.devices = [this] { return bus; };
    access.presence = [this](uint16_t vendor, uint16_t product) {
      if (vendor == capture::kAlteraVendorId &&
          product == capture::kUsbBlasterProductId) {
        return cable_presence;
      }
      return bridge_presence;
    };
    access.open_cable =
        [this](std::string* problem) -> std::unique_ptr<capture::IJtagCable> {
      if (!cable_opens) {
        if (problem != nullptr) {
          *problem = "Quartus's own jtagd holds the cable open.";
        }
        return nullptr;
      }
      return std::make_unique<BorrowedCable>(&cable);
    };
    access.open_programmer =
        [this](
            const std::string&) -> std::unique_ptr<capture::IDeviceProgrammer> {
      return std::make_unique<BorrowedProgrammer>(&programmer);
    };
    access.open_updater =
        [this](const std::string&) -> std::unique_ptr<capture::IDeviceUpdater> {
      return std::make_unique<BorrowedUpdater>(&updater);
    };

    access.bundled_file = [bundled] { return bundled; };

    wizard = std::make_unique<BoardBringUpWizard>(std::move(access));

    // The fixture file is signed with the development key, and whether the
    // default policy accepts one depends on how the build was configured
    // rather than on the flow under test.
    capture::UpdateKeyPolicy policy;
    policy.accept_development_key = true;
    wizard->SetKeyPolicy(std::move(policy));
  }

  QLabel* Label(const char* name) const {
    return wizard->findChild<QLabel*>(QLatin1String(name));
  }
  QPushButton* Button(const char* name) const {
    return wizard->findChild<QPushButton*>(QLatin1String(name));
  }
  QProgressBar* Progress(const char* name) const {
    return wizard->findChild<QProgressBar*>(QLatin1String(name));
  }

  // The one physical act the jumper page waits for: the jumper fitted, both
  // cables out — which is the half this window can actually see — and the
  // board back in its boot ROM.
  void FitTheJumper() {
    bus.clear();
    wizard->Poll();
    bus = {Device(capture::DevicePersonality::kRecovery)};
    wizard->Poll();
  }

  // Walk forward to a page, choosing the update file and fitting the jumper on
  // the way.
  void AdvanceTo(BringUpPage page) {
    for (int guard = 0; guard < 12 && wizard->page() != page; ++guard) {
      if (wizard->page() == BringUpPage::kImage &&
          !Button(BoardBringUpWizard::kNextButtonName)->isEnabled()) {
        wizard->LoadUpdateFile(file.path());
      }
      if (wizard->page() == BringUpPage::kJumper) {
        FitTheJumper();
      }
      wizard->Poll();
      if (!Button(BoardBringUpWizard::kNextButtonName)->isEnabled()) {
        break;
      }
      wizard->GoNext();
    }
  }

  bool RunToCompletion() {
    for (int attempt = 0; attempt < 2000 && wizard->busy(); ++attempt) {
      QTest::qWait(5);
    }
    return !wizard->busy();
  }
};

// --- the ordering ---------------------------------------------------------
//
// The one test in this file that is about the hardware rather than about a user
// interface. The original firmware and the current gateware both drive
// `CTL_07`, so a board running the first under the second has two drivers on
// one net — current-limited by the Explorer Kit's 22 Ω series resistor, and
// still well out of specification for both dies.
//
// What keeps a board out of that pairing is where the FX3 is while the FPGA
// changes: in its boot ROM, with every shared pin idle. So the jumper page
// comes before the configure page, and the configure page before anything is
// written.

TEST(BoardBringUpWizardTest, TheFpgaIsOnlyConfiguredOnceTheFx3IsInItsBootRom) {
  WizardUnderTest test(capture::DevicePersonality::kLegacy);

  const std::vector<BringUpPage> steps = test.wizard->Steps();

  const auto position = [&steps](BringUpPage page) {
    return std::distance(steps.begin(),
                         std::find(steps.begin(), steps.end(), page));
  };

  EXPECT_LT(position(BringUpPage::kJumper), position(BringUpPage::kConfigure));
  EXPECT_LT(position(BringUpPage::kConfigure), position(BringUpPage::kProgram));

  // And everything else about the order, stated once: the physical steps
  // bracket the programming ones, and the power cycle is after all of them.
  EXPECT_LT(position(BringUpPage::kConnect), position(BringUpPage::kImage));
  EXPECT_LT(position(BringUpPage::kProgram),
            position(BringUpPage::kRemoveJumper));
  EXPECT_LT(position(BringUpPage::kRemoveJumper),
            position(BringUpPage::kPowerCycle));
  EXPECT_LT(position(BringUpPage::kPowerCycle), position(BringUpPage::kVerify));
}

// The other half of what makes the bad pairing unreachable, and the half that
// is a property of this class rather than of the engine.
//
// What a board is *running* only changes at a power cycle: the FPGA reloads
// from flash and the FX3 re-reads its boot source, both at that moment and not
// before. So no power cycle may be asked for in the middle of the writing —
// every one has to fall before anything has been written or after all of it
// has, and then the three images become the running ones together.
//
// Asked of both branches, because the branch that fits a jumper is the one that
// asks for an extra power cycle, and it is the one this could go wrong on.
TEST(BoardBringUpWizardTest, NoPowerCycleIsAskedForInTheMiddleOfTheWriting) {
  for (capture::DevicePersonality personality :
       {capture::DevicePersonality::kRecovery,
        capture::DevicePersonality::kLegacy}) {
    WizardUnderTest test(personality);
    test.AdvanceTo(BringUpPage::kImage);

    const std::vector<BringUpPage> steps = test.wizard->Steps();
    const auto position = [&steps](BringUpPage page) {
      return std::distance(steps.begin(),
                           std::find(steps.begin(), steps.end(), page));
    };

    // Every page that asks the user to pull the cables. The jumper page does,
    // because a jumper only takes effect on a boot.
    std::vector<BringUpPage> replugs;
    for (BringUpPage page : steps) {
      if (page == BringUpPage::kJumper || page == BringUpPage::kPowerCycle) {
        replugs.push_back(page);
      }
    }

    for (BringUpPage page : replugs) {
      const bool before_any_writing =
          position(page) < position(BringUpPage::kProgram);
      const bool after_all_writing =
          position(page) > position(BringUpPage::kProgram);
      EXPECT_TRUE(before_any_writing || after_all_writing)
          << "a power cycle is asked for in the middle of the writing, which "
             "is the one window in which a board could come up with some of "
             "its images changed and some not";
    }

    // And the page that sits between the writing and the power cycle says so
    // in as many words.
    EXPECT_TRUE(test.Label(BoardBringUpWizard::kRemoveJumperTextName)
                    ->text()
                    .contains("Do not unplug"));
  }
}

// The engine refuses out of order whatever the interface does, so a page wired
// up wrongly is a refused operation rather than a damaged board.
TEST(BoardBringUpWizardTest,
     TheProgramButtonIsRefusedUntilTheFpgaIsConfigured) {
  WizardUnderTest test;
  test.AdvanceTo(BringUpPage::kConfigure);

  // The wizard will not even walk past the configure page until it is done,
  // which is the first of the two guards.
  ASSERT_EQ(test.wizard->page(), BringUpPage::kConfigure);
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());

  // And the button that would do it is off, wherever the user is.
  EXPECT_FALSE(
      test.Button(BoardBringUpWizard::kProgramStartButtonName)->isEnabled());

  // And pressing it anyway does nothing at all.
  test.wizard->StartProgram();
  EXPECT_FALSE(test.wizard->busy());
  EXPECT_EQ(test.programmer.sections_written(), 0u);
}

// --- the jumper, on every board -------------------------------------------

// The bug this pair of tests exists for. The wizard used to skip both jumper
// pages for a board already reporting its boot ROM, on the reasoning that a
// board already there needs no jumper to get there. That is true of a board
// somebody put there, and false of the commonest board of all: a freshly built
// kit is in its boot ROM because its EEPROM is empty, jumper or no jumper — so
// it comes out again at the first restart, in the middle of the writing.
TEST(BoardBringUpWizardTest, ABoardInItsBootRomIsStillAskedForTheJumper) {
  WizardUnderTest test(capture::DevicePersonality::kRecovery);
  test.AdvanceTo(BringUpPage::kJumper);

  ASSERT_EQ(test.wizard->page(), BringUpPage::kJumper);
  EXPECT_EQ(test.wizard->Steps().size(), 9u);

  const std::vector<BringUpPage> steps = test.wizard->Steps();
  EXPECT_EQ(std::count(steps.begin(), steps.end(), BringUpPage::kJumper), 1);
  EXPECT_EQ(std::count(steps.begin(), steps.end(), BringUpPage::kRemoveJumper),
            1);

  // And it is not waved through on the strength of a personality it had before
  // anybody touched it. Being in the boot ROM is where this board started; what
  // the page waits for is the restart that says a jumper put it there.
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kJumperTextName)
                  ->text()
                  .contains("even if"));

  test.FitTheJumper();
  EXPECT_TRUE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
}

// A legacy board is running firmware, so it has to be sent to the jumper —
// and this is the case the whole wizard exists for.
TEST(BoardBringUpWizardTest, ALegacyBoardIsSentToTheJumper) {
  WizardUnderTest test(capture::DevicePersonality::kLegacy);
  test.AdvanceTo(BringUpPage::kJumper);

  EXPECT_EQ(test.wizard->page(), BringUpPage::kJumper);
  EXPECT_EQ(test.wizard->Steps().size(), 9u);

  // And held there until the board comes back in its boot ROM, because the
  // jumper only takes effect on a boot.
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());

  test.FitTheJumper();
  EXPECT_TRUE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
}

// Stepping back to re-read the page does not ask for the power cycle again.
// The jumper is fitted; it has not become unfitted because somebody wanted
// another look at the photograph.
TEST(BoardBringUpWizardTest, GoingBackToTheJumperPageDoesNotAskForItAgain) {
  WizardUnderTest test(capture::DevicePersonality::kLegacy);
  test.AdvanceTo(BringUpPage::kConfigure);
  ASSERT_EQ(test.wizard->page(), BringUpPage::kConfigure);

  test.wizard->GoPrevious();
  ASSERT_EQ(test.wizard->page(), BringUpPage::kJumper);
  test.wizard->Poll();

  EXPECT_TRUE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
}

// Every photograph page really draws a photograph.
//
// The failure this guards against is silent and was shipped once: the wizard
// tolerates a pixmap that will not load, so a picture that is missing from the
// resource, renamed out from under the path, or in a format the running Qt
// cannot decode leaves an empty box and an application that is otherwise
// perfectly well. Asserting the pixmap rather than the label catches all three.
TEST(BoardBringUpWizardTest, EveryPhotographPageDrawsItsPhotograph) {
  WizardUnderTest test;

  for (const char* name : {BoardBringUpWizard::kOverviewPhotographName,
                           BoardBringUpWizard::kJumperPhotographName,
                           BoardBringUpWizard::kRemoveJumperPhotographName}) {
    QLabel* const label = test.Label(name);
    ASSERT_NE(label, nullptr) << name;
    ASSERT_FALSE(label->pixmap().isNull()) << name;
    EXPECT_GT(label->pixmap().width(), 0) << name;
  }
}

// --- the connectivity page ------------------------------------------------

TEST(BoardBringUpWizardTest, ItWillNotStartWithNothingAttached) {
  WizardUnderTest test;
  test.bus.clear();
  test.cable_presence = capture::UsbPresence::kAbsent;
  test.cable_opens = false;

  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), BringUpPage::kConnect);
  test.wizard->Poll();

  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kFx3RowName)
                  ->text()
                  .contains("Nothing found"));
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kFpgaRowName)
                  ->text()
                  .contains("charge-only", Qt::CaseInsensitive));
}

// The Windows and Quartus case, and the reason the presence check exists: a
// cable that is plainly attached and will not open is a different problem
// from no cable, and the page says which.
TEST(BoardBringUpWizardTest, AnAttachedCableThatWillNotOpenIsNamedAsSuch) {
  WizardUnderTest test;
  test.cable_opens = false;
  test.cable_presence = capture::UsbPresence::kPresent;

  test.wizard->GoNext();
  test.wizard->Poll();

  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
  EXPECT_TRUE(
      test.Label(BoardBringUpWizard::kFpgaRowName)->text().contains("jtagd"));
}

TEST(BoardBringUpWizardTest, TheKitsDebugPortSaysThePowerIsOn) {
  WizardUnderTest test;
  test.bus.clear();
  test.bridge_presence = capture::UsbPresence::kPresent;

  test.wizard->GoNext();
  test.wizard->Poll();

  EXPECT_TRUE(test.Label(BoardBringUpWizard::kFx3RowName)
                  ->text()
                  .contains("has power"));
}

// --- the image page -------------------------------------------------------

TEST(BoardBringUpWizardTest, AnUpdateOnlyFileIsRefusedWithAReason) {
  WizardUnderTest test;
  test.AdvanceTo(BringUpPage::kImage);
  ASSERT_EQ(test.wizard->page(), BringUpPage::kImage);

  test.wizard->LoadUpdateFile(
      BringUpFile::UpdateOnlyPath(test.file.directory()));

  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());

  // Named rather than dismissed: this is a real release bundle that updates a
  // working device perfectly well, so the page says which payloads bring-up
  // needs that it does not have.
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kImageLabelName)
                  ->text()
                  .contains("vectors", Qt::CaseInsensitive));
}

TEST(BoardBringUpWizardTest, ACompleteFileIsAcceptedAndSaysWhatItCarries) {
  WizardUnderTest test;
  test.AdvanceTo(BringUpPage::kImage);

  test.wizard->LoadUpdateFile(test.file.path());

  EXPECT_TRUE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kImageLabelName)
                  ->text()
                  .contains("1.4.0"));

  // The development-signature banner, from the same function the update page
  // uses: it proves the file is well formed and nothing whatever about where
  // it came from, and that has to be said every time.
  EXPECT_FALSE(test.Label(BoardBringUpWizard::kImageBannerName)->isHidden());
}

TEST(BoardBringUpWizardTest, AFileThatIsNotABundleIsRefusedRatherThanCrashing) {
  WizardUnderTest test;
  test.AdvanceTo(BringUpPage::kImage);

  const QString path =
      test.file.directory().filePath(QStringLiteral("rubbish.dddfw"));
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write("not a bundle");
  file.close();

  test.wizard->LoadUpdateFile(path);
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
}

TEST(BoardBringUpWizardTest, TheStatusMarksAreTheCharactersTheyLookLike) {
  // A tick is three bytes of UTF-8. Handing those bytes to something that
  // reads Latin-1 puts three characters of mojibake on the page where one mark
  // belongs — and every test that looks only at the words passes while the
  // page in front of a user is unreadable. So the marks are asserted as
  // characters, and the lead byte of a misread sequence is asserted absent.
  WizardUnderTest waiting(capture::DevicePersonality::kLegacy);
  waiting.AdvanceTo(BringUpPage::kConnect);

  const QString legacy_row =
      waiting.Label(BoardBringUpWizard::kFx3RowName)->text();
  const QString cable_row =
      waiting.Label(BoardBringUpWizard::kFpgaRowName)->text();

  EXPECT_TRUE(legacy_row.contains(QStringLiteral("\u2022")))
      << legacy_row.toStdString();
  EXPECT_TRUE(cable_row.contains(QStringLiteral("\u2713")))
      << cable_row.toStdString();

  // A board in its boot ROM is amber too — usable as it is, and still with a
  // jumper to fit. The tick above, on the cable row, and the cross below cover
  // the other two marks.
  WizardUnderTest ready;
  ready.AdvanceTo(BringUpPage::kConnect);
  const QString boot_rom_row =
      ready.Label(BoardBringUpWizard::kFx3RowName)->text();
  EXPECT_TRUE(boot_rom_row.contains(QStringLiteral("\u2022")))
      << boot_rom_row.toStdString();

  WizardUnderTest nothing;
  nothing.bus.clear();
  nothing.AdvanceTo(BringUpPage::kConnect);
  const QString absent_row =
      nothing.Label(BoardBringUpWizard::kFx3RowName)->text();
  EXPECT_TRUE(absent_row.contains(QStringLiteral("\u2715")))
      << absent_row.toStdString();

  for (const QString& text :
       {legacy_row, cable_row, boot_rom_row, absent_row}) {
    // U+00E2, which is what the first byte of any of these sequences becomes
    // when it is read as Latin-1.
    EXPECT_FALSE(text.contains(QChar(0x00E2))) << text.toStdString();
  }
}

// --- the file a packaged build carries ------------------------------------
//
// A board being brought up cannot be updated over USB — that is what the whole
// wizard is for — so the machine beside it may be one that has just been built
// and has no network. A packaged build therefore installs an update file beside
// itself, and these are the four states that produces.

TEST(BoardBringUpWizardTest, ABundledFileIsChosenForYouAndSaysWhereItCameFrom) {
  BringUpFile packaged;
  WizardUnderTest test(capture::DevicePersonality::kRecovery, packaged.path());

  test.AdvanceTo(BringUpPage::kImage);
  ASSERT_EQ(test.wizard->page(), BringUpPage::kImage);

  EXPECT_TRUE(test.wizard->using_bundled_file());
  EXPECT_TRUE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kImageLabelName)
                  ->text()
                  .contains("1.4.0"));

  // And says so, rather than a file simply appearing in a field. The sentence
  // that matters is the one about it being checked anyway.
  const QString source =
      test.Label(BoardBringUpWizard::kImageSourceName)->text();
  EXPECT_TRUE(source.contains("carries an update file", Qt::CaseInsensitive))
      << source.toStdString();
  EXPECT_TRUE(source.contains("signature", Qt::CaseInsensitive))
      << source.toStdString();

  // Nothing to go back to, so nothing offering it.
  EXPECT_TRUE(
      test.Button(BoardBringUpWizard::kUseBundledButtonName)->isHidden());
}

TEST(BoardBringUpWizardTest, ABundledFileIsVerifiedLikeAnyOther) {
  // The property the whole design turns on: arriving with the application is
  // not a reason to trust a file. A truncated install, or a file somebody
  // replaced, is refused exactly as a downloaded one would be.
  BringUpFile packaged;
  const QString broken =
      packaged.directory().filePath(QStringLiteral("update.dddfw"));

  QFile file(broken);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write("manifest.json, and then nothing that follows one");
  file.close();

  WizardUnderTest test(capture::DevicePersonality::kRecovery, broken);
  test.AdvanceTo(BringUpPage::kImage);

  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kImageSourceName)
                  ->text()
                  .contains("could not be used", Qt::CaseInsensitive));
}

TEST(BoardBringUpWizardTest, AChosenFileReplacesTheBundledOneAndCanBeUndone) {
  BringUpFile packaged;
  WizardUnderTest test(capture::DevicePersonality::kRecovery, packaged.path());
  test.AdvanceTo(BringUpPage::kImage);

  // A file that cannot bring a board up: the bundled one being good does not
  // make a chosen one acceptable.
  test.wizard->LoadUpdateFile(
      BringUpFile::UpdateOnlyPath(test.file.directory()));

  EXPECT_FALSE(test.wizard->using_bundled_file());
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
  EXPECT_FALSE(
      test.Button(BoardBringUpWizard::kUseBundledButtonName)->isHidden());

  test.Button(BoardBringUpWizard::kUseBundledButtonName)->click();

  EXPECT_TRUE(test.wizard->using_bundled_file());
  EXPECT_TRUE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
}

TEST(BoardBringUpWizardTest, ABuildWithNoBundledFileSaysWhereToGetOne) {
  // What a build from source looks like, and what a build whose packaging
  // pinned nothing looks like. The honest state: the file picker, and the name
  // of the file to fetch.
  WizardUnderTest test;
  test.AdvanceTo(BringUpPage::kImage);

  EXPECT_TRUE(test.wizard->bundled_path().isEmpty());
  EXPECT_FALSE(test.wizard->using_bundled_file());

  const QString source =
      test.Label(BoardBringUpWizard::kImageSourceName)->text();
  EXPECT_TRUE(source.contains("no update file", Qt::CaseInsensitive))
      << source.toStdString();
  EXPECT_TRUE(source.contains("domesday-duplicator-update"))
      << source.toStdString();

  EXPECT_TRUE(
      test.Button(BoardBringUpWizard::kUseBundledButtonName)->isHidden());
}

// --- programming ----------------------------------------------------------

TEST(BoardBringUpWizardTest, ItConfiguresThenProgramsAndVerifiesAtTheEnd) {
  WizardUnderTest test;
  test.cable.AnswerWith(capture::QuartusOpeningAnswers());
  test.AdvanceTo(BringUpPage::kConfigure);
  ASSERT_EQ(test.wizard->page(), BringUpPage::kConfigure);

  QSignalSpy busy(test.wizard.get(), &BoardBringUpWizard::BusyChanged);

  test.Button(BoardBringUpWizard::kConfigureStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());

  // Vectors played, and not a byte written: this step is what makes the flash
  // reachable, not what fills it.
  EXPECT_GT(test.cable.clocks(), 0u);
  EXPECT_EQ(test.updater.begin_count(), 0u);
  EXPECT_EQ(test.Progress(BoardBringUpWizard::kConfigureProgressName)->value(),
            100);

  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), BringUpPage::kProgram);

  test.Button(BoardBringUpWizard::kProgramStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());

  // All three images, in the order that makes every interruption recoverable.
  EXPECT_EQ(
      test.updater.begun_targets(),
      (std::vector<capture::UpdateTarget>{capture::UpdateTarget::kFirmware,
                                          capture::UpdateTarget::kEpcsFactory,
                                          capture::UpdateTarget::kGateware}));

  // And nothing restarted: the power cycle two pages on is what starts them.
  EXPECT_EQ(test.updater.reset_count(), 0u);
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kProgramStatusName)
                  ->text()
                  .contains("written and checked"));
  EXPECT_EQ(test.Progress(BoardBringUpWizard::kProgramProgressName)->value(),
            100);

  // The window told the rest of the application to keep out of the way while
  // each half ran, and to come back afterwards.
  EXPECT_GE(busy.count(), 4);

  // The jumper comes off, and then the power cycle, which the wizard waits for
  // rather than performing.
  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), BringUpPage::kRemoveJumper);
  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), BringUpPage::kPowerCycle);
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());

  // The cables coming out, which is the observation the page is actually
  // waiting for. A test that skipped it would be asserting that a device
  // sitting on the bus is proof of a power cycle, which is the bug.
  test.bus.clear();
  test.wizard->Poll();
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());

  // The application image, which is what a finished board comes up on: the
  // factory image validates it at power-on and hands over.
  test.bus = {Device(capture::DevicePersonality::kApplication)};
  test.updater.SetIdentity(BroughtUpIdentity());
  test.wizard->Poll();

  ASSERT_TRUE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
  test.wizard->GoNext();

  ASSERT_EQ(test.wizard->page(), BringUpPage::kVerify);
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kVerifySummaryName)
                  ->text()
                  .contains("Bring-up complete"));
}

// --- the power cycle ------------------------------------------------------
//
// The page that cannot go by what is on the bus. Reported from a bench: it
// announced that both cables had been pulled and reinserted the instant it was
// reached, with nothing touched.
//
// The cause is the step before it. Programming hands the firmware to the FX3's
// boot ROM and runs it out of RAM, so a fully working Duplicator is
// enumerating by the time this page appears — and "is a Duplicator attached?"
// was the whole of what the page asked.

// The bench report, as a test.
TEST(BoardBringUpWizardTest, ThePowerCycleIsNotReportedBeforeItHappens) {
  WizardUnderTest test;
  test.cable.AnswerWith(capture::QuartusOpeningAnswers());
  test.updater.SetIdentity(BroughtUpIdentity());
  test.AdvanceTo(BringUpPage::kConfigure);

  test.Button(BoardBringUpWizard::kConfigureStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());
  test.wizard->GoNext();
  test.Button(BoardBringUpWizard::kProgramStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());

  // What the bench looks like at this exact moment: the FX3 is running the
  // firmware it was just handed, out of RAM, and enumerates as a Duplicator.
  test.bus = {Device(capture::DevicePersonality::kApplication)};
  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), BringUpPage::kRemoveJumper);
  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), BringUpPage::kPowerCycle);

  for (int poll = 0; poll < 5; ++poll) {
    test.wizard->Poll();
  }

  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled())
      << "the page reported a power cycle that had not happened";

  const QString waiting =
      test.Label(BoardBringUpWizard::kPowerCycleStatusName)->text();
  EXPECT_FALSE(waiting.contains("All done")) << waiting.toStdString();
  EXPECT_TRUE(waiting.contains("Waiting for", Qt::CaseInsensitive));

  // Gone, and then back on its own flash, which is the real thing.
  test.bus.clear();
  test.wizard->Poll();
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());

  test.bus = {Device(capture::DevicePersonality::kApplication)};
  test.wizard->Poll();

  EXPECT_TRUE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kPowerCycleStatusName)
                  ->text()
                  .contains("All done"));
}

// The failure this page has always warned about, now caught rather than
// guessed at. Pulling the USB 3.0 cable alone makes the device vanish and come
// back while the mini-USB keeps the board alive — so the firmware in RAM and
// the gateware in the FPGA both survive, and every outward sign is of a power
// cycle that worked.
//
// What gives it away is the image role: the gateware step put the *factory*
// image into the FPGA over JTAG, and only a real power cycle replaces it with
// the application image out of flash.
TEST(BoardBringUpWizardTest, APartialPowerCycleIsCaughtByTheImageRole) {
  WizardUnderTest test;
  test.cable.AnswerWith(capture::QuartusOpeningAnswers());
  test.AdvanceTo(BringUpPage::kConfigure);

  test.Button(BoardBringUpWizard::kConfigureStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());
  test.wizard->GoNext();
  test.Button(BoardBringUpWizard::kProgramStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());
  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), BringUpPage::kRemoveJumper);
  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), BringUpPage::kPowerCycle);

  // The board never restarted, so the FPGA still holds what JTAG put there.
  capture::DeviceIdentity jtag_loaded = BroughtUpIdentity();
  jtag_loaded.image_role = capture::kImageRoleFactory;
  test.updater.SetIdentity(jtag_loaded);

  test.bus.clear();
  test.wizard->Poll();
  test.bus = {Device(capture::DevicePersonality::kApplication)};
  test.wizard->Poll();

  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled())
      << "a board that never lost power was accepted as power-cycled";

  const QString status =
      test.Label(BoardBringUpWizard::kPowerCycleStatusName)->text();
  EXPECT_TRUE(status.contains("did not lose power", Qt::CaseInsensitive))
      << status.toStdString();
  EXPECT_TRUE(status.contains("both", Qt::CaseInsensitive));
}

// A board that comes back in its boot ROM says exactly one thing, and nothing
// else puts it there — so it is a diagnosis rather than a timeout.
TEST(BoardBringUpWizardTest, ABoardThatComesBackInItsBootRomNamesTheJumper) {
  WizardUnderTest test(capture::DevicePersonality::kLegacy);
  test.cable.AnswerWith(capture::QuartusOpeningAnswers());
  test.updater.SetIdentity(BroughtUpIdentity());
  test.AdvanceTo(BringUpPage::kConfigure);

  test.Button(BoardBringUpWizard::kConfigureStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());
  test.wizard->GoNext();
  test.Button(BoardBringUpWizard::kProgramStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());
  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), BringUpPage::kRemoveJumper);
  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), BringUpPage::kPowerCycle);

  // Cables out, cables in — and it comes back where the jumper puts it.
  test.bus.clear();
  test.wizard->Poll();
  test.bus = {Device(capture::DevicePersonality::kRecovery)};
  test.wizard->Poll();

  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kPowerCycleStatusName)
                  ->text()
                  .contains("J4"));
}

// The device did not come back. The first thing said is the partial power
// cycle, because it is the commonest cause and the one whose symptom is that
// everything looks correct.
TEST(BoardBringUpWizardTest, APowerCycleThatNeverHappenedAsksAboutBothCables) {
  WizardUnderTest test;
  test.cable.AnswerWith(capture::QuartusOpeningAnswers());
  test.AdvanceTo(BringUpPage::kConfigure);

  test.Button(BoardBringUpWizard::kConfigureStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());
  test.wizard->GoNext();

  test.Button(BoardBringUpWizard::kProgramStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());
  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), BringUpPage::kRemoveJumper);
  test.wizard->GoNext();

  ASSERT_EQ(test.wizard->page(), BringUpPage::kPowerCycle);
  for (int poll = 0; poll < 25; ++poll) {
    test.wizard->Poll();
  }

  EXPECT_TRUE(test.Label(BoardBringUpWizard::kPowerCycleStatusName)
                  ->text()
                  .contains("both cables", Qt::CaseInsensitive));
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
}

// A failure at either half is safe and says so. This one cannot be arranged on
// a bench at all: it is a boot ROM that takes an image and will not run it.
TEST(BoardBringUpWizardTest, AFailedProgramStepSaysNothingIsBroken) {
  WizardUnderTest test;
  test.cable.AnswerWith(capture::QuartusOpeningAnswers());
  test.programmer.SetFault(FakeDeviceProgrammer::Fault::kRefuseStart);
  test.AdvanceTo(BringUpPage::kConfigure);

  test.Button(BoardBringUpWizard::kConfigureStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());
  test.wizard->GoNext();

  test.Button(BoardBringUpWizard::kProgramStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());

  const QString status =
      test.Label(BoardBringUpWizard::kProgramStatusName)->text();
  EXPECT_TRUE(status.contains("J4"));
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());

  // And offered again, because every step of this can simply be run again.
  EXPECT_TRUE(
      test.Button(BoardBringUpWizard::kProgramStartButtonName)->isEnabled());
}

TEST(BoardBringUpWizardTest, AStoppedConfigureIsNotReportedAsAFailure) {
  WizardUnderTest test;
  test.cable.AnswerWith(capture::QuartusOpeningAnswers());
  test.AdvanceTo(BringUpPage::kConfigure);

  test.Button(BoardBringUpWizard::kConfigureStartButtonName)->click();
  test.wizard->Stop();
  ASSERT_TRUE(test.RunToCompletion());

  const QString status =
      test.Label(BoardBringUpWizard::kConfigureStatusName)->text();
  EXPECT_TRUE(status.contains("Stopped"));
  EXPECT_FALSE(status.contains("failed", Qt::CaseInsensitive));
}

// --- what each step says about itself --------------------------------------

// A finished step has to be finished in every way the page can show it: the
// button that did the work is dead *and* relabelled, and the line under it
// leads with All done and names what to press next. Greyed out on its own
// reads as "not yet"; greyed out and relabelled reads as "already".
TEST(BoardBringUpWizardTest, AFinishedStepDisablesItsButtonAndSaysAllDone) {
  WizardUnderTest test;
  test.cable.AnswerWith(capture::QuartusOpeningAnswers());
  test.AdvanceTo(BringUpPage::kConfigure);

  QPushButton* const start =
      test.Button(BoardBringUpWizard::kConfigureStartButtonName);
  QLabel* const status = test.Label(BoardBringUpWizard::kConfigureStatusName);

  // Before: offered, and saying what it is waiting for rather than nothing.
  ASSERT_TRUE(start->isEnabled());
  const QString before = start->text();
  EXPECT_TRUE(status->text().contains("Waiting for", Qt::CaseInsensitive))
      << status->text().toStdString();

  start->click();
  ASSERT_TRUE(test.RunToCompletion());

  EXPECT_FALSE(start->isEnabled());
  EXPECT_NE(start->text(), before)
      << "a button that had already been pressed still offered to do the work";
  EXPECT_TRUE(status->text().contains("All done"))
      << status->text().toStdString();
  EXPECT_TRUE(status->text().contains("Next"));

  // And the same for the step that writes, which is the one somebody is most
  // anxious to know has finished.
  test.wizard->GoNext();
  QPushButton* const program =
      test.Button(BoardBringUpWizard::kProgramStartButtonName);
  program->click();
  ASSERT_TRUE(test.RunToCompletion());

  EXPECT_FALSE(program->isEnabled());
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kProgramStatusName)
                  ->text()
                  .contains("All done"));
}

// A page that is not finished says what it is waiting for, rather than leaving
// a dead Next button as the only sign that something is outstanding.
TEST(BoardBringUpWizardTest, APageThatIsNotFinishedSaysWhatItIsWaitingFor) {
  WizardUnderTest stuck;
  stuck.bus.clear();
  stuck.cable_opens = false;
  stuck.cable_presence = capture::UsbPresence::kAbsent;
  stuck.wizard->GoNext();
  stuck.wizard->Poll();

  ASSERT_EQ(stuck.wizard->page(), BringUpPage::kConnect);
  EXPECT_TRUE(stuck.Label(BoardBringUpWizard::kConnectStatusName)
                  ->text()
                  .contains("Waiting for", Qt::CaseInsensitive));

  // And the same page, satisfied, says so in the same place — so the two are
  // never both on screen and never both absent.
  WizardUnderTest ready;
  ready.AdvanceTo(BringUpPage::kConnect);
  ready.wizard->Poll();

  ASSERT_EQ(ready.wizard->page(), BringUpPage::kConnect);
  EXPECT_TRUE(ready.Label(BoardBringUpWizard::kConnectStatusName)
                  ->text()
                  .contains("All done"));

  // The file page, which is waiting for something a user has to go and find.
  ready.wizard->GoNext();
  ASSERT_EQ(ready.wizard->page(), BringUpPage::kImage);
  EXPECT_TRUE(ready.Label(BoardBringUpWizard::kImageStatusName)
                  ->text()
                  .contains("Waiting for", Qt::CaseInsensitive));

  ready.wizard->LoadUpdateFile(ready.file.path());
  EXPECT_TRUE(ready.Label(BoardBringUpWizard::kImageStatusName)
                  ->text()
                  .contains("All done"));
}

// A run that failed leaves its own sentence on the page. The status line is
// otherwise derived from the state in hand, and that state — nothing written,
// button offered again — is indistinguishable from never having started.
TEST(BoardBringUpWizardTest, AFailureIsNotReplacedByTheWaitingLine) {
  WizardUnderTest test;
  test.cable.AnswerWith(capture::QuartusOpeningAnswers());
  test.programmer.SetFault(FakeDeviceProgrammer::Fault::kRefuseStart);
  test.AdvanceTo(BringUpPage::kConfigure);

  test.Button(BoardBringUpWizard::kConfigureStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());
  test.wizard->GoNext();

  test.Button(BoardBringUpWizard::kProgramStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());

  // Several polls later, and after navigating away and back, the page still
  // says what happened.
  test.wizard->Poll();
  test.wizard->GoPrevious();
  test.wizard->GoNext();

  const QString status =
      test.Label(BoardBringUpWizard::kProgramStatusName)->text();
  EXPECT_TRUE(status.contains("J4")) << status.toStdString();
  EXPECT_FALSE(status.contains("Waiting for", Qt::CaseInsensitive))
      << "a failed run was quietly replaced by an invitation to start one";
}

// --- navigation and leaving ------------------------------------------------

TEST(BoardBringUpWizardTest, ItOpensOnTheOverviewWithEverythingItWillAskFor) {
  const WizardUnderTest test;

  EXPECT_EQ(test.wizard->page(), BringUpPage::kOverview);
  ASSERT_NE(test.Label(BoardBringUpWizard::kOverviewTextName), nullptr);
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kOverviewTextName)
                  ->text()
                  .contains("both cables", Qt::CaseInsensitive));
  EXPECT_FALSE(
      test.Button(BoardBringUpWizard::kPreviousButtonName)->isEnabled());
}

TEST(BoardBringUpWizardTest, EveryPageIsBuiltAndNamed) {
  const WizardUnderTest test;

  for (const char* name :
       {BoardBringUpWizard::kOverviewTextName, BoardBringUpWizard::kFx3RowName,
        BoardBringUpWizard::kFpgaRowName,
        BoardBringUpWizard::kConnectLegendName,
        BoardBringUpWizard::kConnectStatusName,
        BoardBringUpWizard::kImageLabelName,
        BoardBringUpWizard::kImageStatusName,
        BoardBringUpWizard::kJumperTextName,
        BoardBringUpWizard::kJumperStatusName,
        BoardBringUpWizard::kConfigureTextName,
        BoardBringUpWizard::kConfigureStatusName,
        BoardBringUpWizard::kProgramTextName,
        BoardBringUpWizard::kProgramStatusName,
        BoardBringUpWizard::kRemoveJumperTextName,
        BoardBringUpWizard::kRemoveJumperStatusName,
        BoardBringUpWizard::kPowerCycleTextName,
        BoardBringUpWizard::kPowerCycleStatusName,
        BoardBringUpWizard::kVerifyTextName}) {
    EXPECT_NE(test.Label(name), nullptr) << name;
  }
}

// Nothing follows a bring-up, so the last page offers nothing to press. The
// flow used to hand over to the update dialog, and a button that still did
// would send somebody to repeat the work they had just finished.
TEST(BoardBringUpWizardTest, TheLastPageHandsOverToNothing) {
  const WizardUnderTest test;

  EXPECT_EQ(test.Button("bringup_update_now"), nullptr);
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kVerifyTextName) != nullptr);
}

}  // namespace
}  // namespace ddd::gui
