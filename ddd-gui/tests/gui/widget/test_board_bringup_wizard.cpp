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

// A signed provisioning set on disk. Built from the same fixtures the engine
// tests use, because a wizard that only ever met a file its own test wrote
// would meet the real format for the first time on somebody's bench.
class ProvisioningFile {
 public:
  ProvisioningFile() {
    capture::UstarWriter writer;
    writer.AddFile(
        capture::kManifestEntryName,
        capture::test::Bytes(capture::test::kProvisioningManifestJson));
    writer.AddFile(
        capture::kSignatureEntryName,
        capture::test::Bytes(capture::test::kProvisioningManifestSignature));
    writer.AddFile(capture::kFirmwareEntryName, capture::test::MakeBootImage());
    writer.AddFile(capture::kProvisioningEntryName,
                   capture::test::Bytes(capture::test::kProvisioningPayload));
    writer.AddFile(
        capture::kFactoryGatewareEntryName,
        capture::test::Bytes(capture::test::kFactoryGatewarePayload));
    Write(writer.Finish(), QStringLiteral("provisioning.dddfw"));
  }

  // An ordinary update file, which is the wrong file for this window and is
  // the mistake somebody is most likely to make.
  static QString UpdateBundlePath(QTemporaryDir& directory) {
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
    const QString path = directory.filePath(QStringLiteral("update.dddfw"));
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

  ProvisioningFile file;
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

    access.bundled_set = [bundled] { return bundled; };

    wizard = std::make_unique<BoardBringUpWizard>(std::move(access));

    // The fixture set is signed with the development key, and whether the
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

  // Walk forward to a page, choosing the provisioning set on the way.
  void AdvanceTo(BringUpPage page) {
    for (int guard = 0; guard < 12 && wizard->page() != page; ++guard) {
      if (wizard->page() == BringUpPage::kImage &&
          !Button(BoardBringUpWizard::kNextButtonName)->isEnabled()) {
        wizard->LoadProvisioningSet(file.path());
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
// interface. Bring-up must never touch the FPGA before the FX3: the original
// firmware and the current gateware both drive `CTL_07`, so a board running the
// first under the second has two drivers on one net — current-limited by the
// Explorer Kit's 22 Ω series resistor, and still well out of specification for
// both dies.

TEST(BoardBringUpWizardTest, TheFpgaIsNeverProgrammedBeforeTheFx3) {
  const WizardUnderTest test;

  const std::vector<BringUpPage> steps = test.wizard->Steps();

  const auto position = [&steps](BringUpPage page) {
    return std::distance(steps.begin(),
                         std::find(steps.begin(), steps.end(), page));
  };

  EXPECT_LT(position(BringUpPage::kFirmware), position(BringUpPage::kGateware));

  // And everything else about the order, stated once: the physical steps
  // bracket the programming ones, and the power cycle is after both.
  EXPECT_LT(position(BringUpPage::kConnect), position(BringUpPage::kImage));
  EXPECT_LT(position(BringUpPage::kGateware),
            position(BringUpPage::kPowerCycle));
  EXPECT_LT(position(BringUpPage::kPowerCycle), position(BringUpPage::kVerify));
}

// The other half of what makes the bad pairing unreachable, and the half that
// is a property of this class rather than of the engine.
//
// What a board is *running* only changes at a power cycle: the FPGA reloads
// from flash and the FX3 re-reads its boot source, both at that moment and not
// before. So as long as every power cycle this flow asks for happens either
// before anything has been programmed or after both halves have, the two change
// together and there is no window in between — whatever order the pages are in.
//
// Asked of both branches, because the branch that fits a jumper is the one that
// asks for an extra power cycle, and it is the one this could go wrong on.
TEST(BoardBringUpWizardTest, NoPowerCycleIsAskedForBetweenTheTwoHalves) {
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
      const bool before_any_programming =
          position(page) < position(BringUpPage::kFirmware);
      const bool after_both = position(page) > position(BringUpPage::kGateware);
      EXPECT_TRUE(before_any_programming || after_both)
          << "a power cycle is asked for between programming the FX3 and "
             "programming the FPGA, which is the one window in which a board "
             "could come up running the original firmware over current "
             "gateware";
    }

    // And the page that does sit between them says so in as many words.
    EXPECT_TRUE(test.Label(BoardBringUpWizard::kRemoveJumperTextName)
                    ->text()
                    .contains("Do not unplug"));
  }
}

// The engine refuses out of order whatever the interface does, so a page wired
// up wrongly is a refused operation rather than a damaged board.
TEST(BoardBringUpWizardTest, TheFpgaButtonIsRefusedUntilTheFx3IsDone) {
  WizardUnderTest test;
  test.AdvanceTo(BringUpPage::kFirmware);

  // The wizard will not even walk past the FX3 page until that half is done,
  // which is the first of the two guards.
  ASSERT_EQ(test.wizard->page(), BringUpPage::kFirmware);
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());

  // And the button that would do it is off, wherever the user is.
  EXPECT_FALSE(
      test.Button(BoardBringUpWizard::kGatewareStartButtonName)->isEnabled());

  // And pressing it anyway does nothing at all.
  test.wizard->StartGateware();
  EXPECT_FALSE(test.wizard->busy());
  EXPECT_EQ(test.cable.clocks(), 0u);
}

// --- the two branches -----------------------------------------------------

// A freshly built kit arrives in its boot ROM, so there is no jumper to fit —
// and asking for one would be asking somebody to fit a jumper so that the
// wizard could ask them to take it off again.
TEST(BoardBringUpWizardTest, ABoardInItsBootRomSkipsBothJumperPages) {
  WizardUnderTest test(capture::DevicePersonality::kRecovery);
  test.AdvanceTo(BringUpPage::kFirmware);

  EXPECT_FALSE(test.wizard->jumper_needed());
  EXPECT_EQ(test.wizard->page(), BringUpPage::kFirmware);

  const std::vector<BringUpPage> steps = test.wizard->Steps();
  EXPECT_EQ(std::count(steps.begin(), steps.end(), BringUpPage::kJumper), 0);
  EXPECT_EQ(std::count(steps.begin(), steps.end(), BringUpPage::kRemoveJumper),
            0);
  EXPECT_EQ(steps.size(), 7u);
}

// A legacy board is running firmware, so it has to be sent to the jumper —
// and this is the case the whole wizard exists for.
TEST(BoardBringUpWizardTest, ALegacyBoardIsSentToTheJumper) {
  WizardUnderTest test(capture::DevicePersonality::kLegacy);
  test.AdvanceTo(BringUpPage::kJumper);

  EXPECT_TRUE(test.wizard->jumper_needed());
  EXPECT_EQ(test.wizard->page(), BringUpPage::kJumper);
  EXPECT_EQ(test.wizard->Steps().size(), 9u);

  // And held there until the board comes back in its boot ROM, because the
  // jumper only takes effect on a boot.
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());

  test.bus = {Device(capture::DevicePersonality::kRecovery)};
  test.wizard->Poll();
  EXPECT_TRUE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
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

TEST(BoardBringUpWizardTest, AnOrdinaryUpdateFileIsRefusedWithAReason) {
  WizardUnderTest test;
  test.AdvanceTo(BringUpPage::kImage);
  ASSERT_EQ(test.wizard->page(), BringUpPage::kImage);

  test.wizard->LoadProvisioningSet(
      ProvisioningFile::UpdateBundlePath(test.file.directory()));

  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kImageLabelName)
                  ->text()
                  .contains("provisioning", Qt::CaseInsensitive));
}

TEST(BoardBringUpWizardTest, AProvisioningSetIsAcceptedAndSaysWhatItCarries) {
  WizardUnderTest test;
  test.AdvanceTo(BringUpPage::kImage);

  test.wizard->LoadProvisioningSet(test.file.path());

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

  test.wizard->LoadProvisioningSet(path);
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

  // A board in its boot ROM ticks, and nothing attached crosses, so all three
  // marks are covered.
  WizardUnderTest ready;
  ready.AdvanceTo(BringUpPage::kConnect);
  const QString boot_rom_row =
      ready.Label(BoardBringUpWizard::kFx3RowName)->text();
  EXPECT_TRUE(boot_rom_row.contains(QStringLiteral("\u2713")))
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

// --- the set a packaged build carries -------------------------------------
//
// A board being brought up cannot be updated over USB — that is what the whole
// wizard is for — so the machine beside it may be one that has just been built
// and has no network. A packaged build therefore installs a provisioning set
// beside itself, and these are the four states that produces.

TEST(BoardBringUpWizardTest, ABundledSetIsChosenForYouAndSaysWhereItCameFrom) {
  ProvisioningFile packaged;
  WizardUnderTest test(capture::DevicePersonality::kRecovery, packaged.path());

  test.AdvanceTo(BringUpPage::kImage);
  ASSERT_EQ(test.wizard->page(), BringUpPage::kImage);

  EXPECT_TRUE(test.wizard->using_bundled_set());
  EXPECT_TRUE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kImageLabelName)
                  ->text()
                  .contains("1.4.0"));

  // And says so, rather than a set simply appearing in a file field. The
  // sentence that matters is the one about it being checked anyway.
  const QString source =
      test.Label(BoardBringUpWizard::kImageSourceName)->text();
  EXPECT_TRUE(
      source.contains("carries a provisioning set", Qt::CaseInsensitive))
      << source.toStdString();
  EXPECT_TRUE(source.contains("signature", Qt::CaseInsensitive))
      << source.toStdString();

  // Nothing to go back to, so nothing offering it.
  EXPECT_TRUE(
      test.Button(BoardBringUpWizard::kUseBundledButtonName)->isHidden());
}

TEST(BoardBringUpWizardTest, ABundledSetIsVerifiedLikeAnyOtherFile) {
  // The property the whole design turns on: arriving with the application is
  // not a reason to trust a file. A truncated install, or a file somebody
  // replaced, is refused exactly as a downloaded one would be.
  ProvisioningFile packaged;
  const QString broken =
      packaged.directory().filePath(QStringLiteral("provisioning.dddfw"));

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

TEST(BoardBringUpWizardTest, AChosenFileReplacesTheBundledSetAndCanBeUndone) {
  ProvisioningFile packaged;
  WizardUnderTest test(capture::DevicePersonality::kRecovery, packaged.path());
  test.AdvanceTo(BringUpPage::kImage);

  // An ordinary update file, which is the wrong file for this window: the
  // bundled one being good does not make a chosen one acceptable.
  test.wizard->LoadProvisioningSet(
      ProvisioningFile::UpdateBundlePath(test.file.directory()));

  EXPECT_FALSE(test.wizard->using_bundled_set());
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
  EXPECT_FALSE(
      test.Button(BoardBringUpWizard::kUseBundledButtonName)->isHidden());

  test.Button(BoardBringUpWizard::kUseBundledButtonName)->click();

  EXPECT_TRUE(test.wizard->using_bundled_set());
  EXPECT_TRUE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
}

TEST(BoardBringUpWizardTest, ABuildWithNoBundledSetSaysWhereToGetOne) {
  // What a build from source looks like, and what a build whose packaging
  // pinned nothing looks like. The honest state: the file picker, and the name
  // of the file to fetch.
  WizardUnderTest test;
  test.AdvanceTo(BringUpPage::kImage);

  EXPECT_TRUE(test.wizard->bundled_path().isEmpty());
  EXPECT_FALSE(test.wizard->using_bundled_set());

  const QString source =
      test.Label(BoardBringUpWizard::kImageSourceName)->text();
  EXPECT_TRUE(source.contains("no provisioning set", Qt::CaseInsensitive))
      << source.toStdString();
  EXPECT_TRUE(source.contains("domesday-duplicator-provisioning"))
      << source.toStdString();

  EXPECT_TRUE(
      test.Button(BoardBringUpWizard::kUseBundledButtonName)->isHidden());
}

// --- programming ----------------------------------------------------------

TEST(BoardBringUpWizardTest, ItProgramsBothHalvesAndVerifiesAtTheEnd) {
  WizardUnderTest test;
  test.cable.AnswerWith(capture::QuartusOpeningAnswers());
  test.AdvanceTo(BringUpPage::kFirmware);
  ASSERT_EQ(test.wizard->page(), BringUpPage::kFirmware);

  QSignalSpy busy(test.wizard.get(), &BoardBringUpWizard::BusyChanged);

  test.Button(BoardBringUpWizard::kFirmwareStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());

  // The jumper is still fitted on a board that needed one, so the device is
  // deliberately not restarted here.
  EXPECT_EQ(test.updater.reset_count(), 0u);
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kFirmwareStatusName)
                  ->text()
                  .contains("written and checked"));
  EXPECT_EQ(test.Progress(BoardBringUpWizard::kFirmwareProgressName)->value(),
            100);

  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), BringUpPage::kGateware);

  test.Button(BoardBringUpWizard::kGatewareStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());

  EXPECT_GT(test.cable.clocks(), 0u);
  EXPECT_EQ(test.Progress(BoardBringUpWizard::kGatewareProgressName)->value(),
            100);

  // The window told the rest of the application to keep out of the way while
  // each half ran, and to come back afterwards.
  EXPECT_GE(busy.count(), 4);

  // The power cycle, which the wizard waits for rather than performing.
  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), BringUpPage::kPowerCycle);
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());

  test.bus = {Device(capture::DevicePersonality::kApplication)};
  test.updater.SetIdentity([] {
    capture::DeviceIdentity identity;
    identity.product_string = "Domesday Duplicator (0123abcd)";
    identity.protocol_version = 1;
    identity.gateware_present = true;
    identity.register_map_version = capture::kRegisterMapVersionWithImageRole;
    identity.image_role = capture::kImageRoleFactory;
    return identity;
  }());
  test.wizard->Poll();

  ASSERT_TRUE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());
  test.wizard->GoNext();

  ASSERT_EQ(test.wizard->page(), BringUpPage::kVerify);
  EXPECT_TRUE(test.Label(BoardBringUpWizard::kVerifySummaryName)
                  ->text()
                  .contains("Bring-up complete"));
}

// The device did not come back. The first thing said is the partial power
// cycle, because it is the commonest cause and the one whose symptom is that
// everything looks correct.
TEST(BoardBringUpWizardTest, APowerCycleThatNeverHappenedAsksAboutBothCables) {
  WizardUnderTest test;
  test.AdvanceTo(BringUpPage::kFirmware);

  test.Button(BoardBringUpWizard::kFirmwareStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());
  test.wizard->GoNext();

  test.cable.AnswerWith(capture::QuartusOpeningAnswers());
  test.Button(BoardBringUpWizard::kGatewareStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());
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
TEST(BoardBringUpWizardTest, AFailedFx3StepSaysNothingIsBroken) {
  WizardUnderTest test;
  test.programmer.SetFault(FakeDeviceProgrammer::Fault::kRefuseStart);
  test.AdvanceTo(BringUpPage::kFirmware);

  test.Button(BoardBringUpWizard::kFirmwareStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());

  const QString status =
      test.Label(BoardBringUpWizard::kFirmwareStatusName)->text();
  EXPECT_TRUE(status.contains("J4"));
  EXPECT_FALSE(test.Button(BoardBringUpWizard::kNextButtonName)->isEnabled());

  // And offered again, because every step of this can simply be run again.
  EXPECT_TRUE(
      test.Button(BoardBringUpWizard::kFirmwareStartButtonName)->isEnabled());
}

TEST(BoardBringUpWizardTest, AStoppedGatewarePlayIsNotReportedAsAFailure) {
  WizardUnderTest test;
  test.cable.AnswerWith(capture::QuartusOpeningAnswers());
  test.AdvanceTo(BringUpPage::kFirmware);

  test.Button(BoardBringUpWizard::kFirmwareStartButtonName)->click();
  ASSERT_TRUE(test.RunToCompletion());
  test.wizard->GoNext();

  test.Button(BoardBringUpWizard::kGatewareStartButtonName)->click();
  test.wizard->Stop();
  ASSERT_TRUE(test.RunToCompletion());

  const QString status =
      test.Label(BoardBringUpWizard::kGatewareStatusName)->text();
  EXPECT_TRUE(status.contains("Stopped"));
  EXPECT_FALSE(status.contains("failed", Qt::CaseInsensitive));
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
        BoardBringUpWizard::kImageLabelName,
        BoardBringUpWizard::kJumperTextName,
        BoardBringUpWizard::kFirmwareTextName,
        BoardBringUpWizard::kRemoveJumperTextName,
        BoardBringUpWizard::kGatewareTextName,
        BoardBringUpWizard::kPowerCycleTextName,
        BoardBringUpWizard::kVerifyTextName}) {
    EXPECT_NE(test.Label(name), nullptr) << name;
  }
}

TEST(BoardBringUpWizardTest, TheLastPageOffersTheUpdateDialog) {
  WizardUnderTest test;
  QSignalSpy asked(test.wizard.get(), &BoardBringUpWizard::OpenUpdateRequested);

  // The button is on the last page and nothing else reaches it, so what is
  // asserted here is only that pressing it asks the window that owns both — the
  // programming that gets somebody to that page is covered above.
  test.Button(BoardBringUpWizard::kUpdateNowButtonName)->click();
  EXPECT_EQ(asked.count(), 1);
}

}  // namespace
}  // namespace ddd::gui
