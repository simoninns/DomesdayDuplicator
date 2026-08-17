/************************************************************************

    capture_plan_form.cpp

    What to capture off a disc that has been examined
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_plan_form.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <filesystem>

#include "capture_settings.h"
#include "free_space.h"
#include "player_text.h"
#include "statistics_presenter.h"

namespace ddd::gui {
namespace {

// The whole disc, where the examination measured it — the fallback bound for a
// field whose end was never established. Validation refuses such a plan
// anyway; this only keeps the spin box from having a nonsensical range while it
// says so.
constexpr int32_t kMaximumFrame = 100000;
constexpr int32_t kMaximumTimeCode = 9999999;

int32_t ProgrammeStart(const player::DiscProfile& disc) {
  return disc.programme_start.known() ? disc.programme_start.value : 0;
}

int32_t ProgrammeEnd(const player::DiscProfile& disc, bool clv) {
  if (disc.programme_end.known()) {
    return disc.programme_end.value;
  }
  return clv ? kMaximumTimeCode : kMaximumFrame;
}

}  // namespace

CapturePlanForm::CapturePlanForm(const player::DiscProfile& disc,
                                 QWidget* parent)
    : QWidget(parent), disc_(disc) {
  clv_ = disc_.disc_type.known() &&
         disc_.disc_type.value == player::DiscType::kClv;

  auto* layout = new QVBoxLayout(this);

  // No margins of its own: this is a form to be placed inside something, and
  // whatever places it owns the spacing around it.
  layout->setContentsMargins(0, 0, 0, 0);

  // --- What to capture -----------------------------------------------------

  auto* shape_box = new QGroupBox(tr("What to capture"), this);
  auto* shape_layout = new QVBoxLayout(shape_box);

  whole_side_ = new QRadioButton(
      CaptureShapeName(player::CaptureShape::kWholeSide), shape_box);
  whole_side_->setObjectName(QLatin1String(kWholeSideRadioName));
  whole_side_->setChecked(true);
  shape_layout->addWidget(whole_side_);

  range_ = new QRadioButton(CaptureShapeName(player::CaptureShape::kRange),
                            shape_box);
  range_->setObjectName(QLatin1String(kRangeRadioName));
  shape_layout->addWidget(range_);

  from_spin_up_ = new QRadioButton(
      CaptureShapeName(player::CaptureShape::kFromSpinUp), shape_box);
  from_spin_up_->setObjectName(QLatin1String(kFromSpinUpRadioName));
  shape_layout->addWidget(from_spin_up_);

  // All three are always on offer. Nothing here asks whether the lead-in can be
  // reached, because no command puts a player there: the two shapes that hold
  // it get it by starting the capture before the disc, which works on any
  // player that can be stopped and started.
  whole_side_->setToolTip(
      tr("The capture is started with the disc stopped, so the spin-up is in "
         "the file; and the disc is spun down again before the capture stops, "
         "so the run-out is too. Neither is an address, and this is the only "
         "way either reaches a file."));
  range_->setToolTip(
      tr("The player seeks to the first address before the capture starts, so "
         "the disc is already turning throughout."));
  from_spin_up_->setToolTip(
      tr("The front of a whole-side capture: the spin-up, and then as much of "
         "the side as you ask for."));

  auto* address_form = new QFormLayout();
  BuildAddressControls(shape_box, address_form);
  shape_layout->addLayout(address_form);

  layout->addWidget(shape_box);

  // --- What the examination could not say ----------------------------------

  auto* form = new QFormLayout();

  if (!disc_.video_standard.known()) {
    standard_ = new QComboBox(this);
    standard_->setObjectName(QLatin1String(kStandardComboName));
    standard_->addItem(tr("Not known"),
                       static_cast<int>(player::VideoStandard::kUnknown));
    standard_->addItem(VideoStandardName(player::VideoStandard::kNtsc),
                       static_cast<int>(player::VideoStandard::kNtsc));
    standard_->addItem(VideoStandardName(player::VideoStandard::kPal),
                       static_cast<int>(player::VideoStandard::kPal));

    // Asked for, and said to be asked for. Nothing is prefilled here because
    // there is nothing to prefill it from: the disc status reads the same for a
    // PAL and an NTSC disc, and the model does not imply it either — this
    // project's own LD-V4300D plays both. It changes no capture; it is what
    // turns a frame count into a playing time and a size.
    standard_->setToolTip(
        tr("This player could not be asked which standard the disc carries. "
           "Saying so here only affects the estimates below — never what is "
           "captured."));
    form->addRow(tr("Video standard"), standard_);
  }

  key_lock_ =
      new QCheckBox(tr("Lock the player's front panel while capturing"), this);
  key_lock_->setObjectName(QLatin1String(kKeyLockCheckName));
  key_lock_->setToolTip(
      tr("Stops a hand on the player pausing a capture halfway through a side. "
         "It is released when the capture ends — but a link that fails with it "
         "on leaves the panel locked, which is why it is not on by default."));
  form->addRow(QString(), key_lock_);

  stop_capture_ =
      new QCheckBox(tr("Stop the capture when the player stops"), this);
  stop_capture_->setObjectName(QLatin1String(kStopCaptureCheckName));
  stop_capture_->setToolTip(
      tr("Off by default on purpose: a player that briefly reports a stopped "
         "state partway through a side would otherwise truncate a capture that "
         "was going perfectly well."));
  form->addRow(QString(), stop_capture_);

  layout->addLayout(form);

  // --- What it will cost, and what is wrong with it ------------------------

  estimate_ = new QLabel(this);
  estimate_->setObjectName(QLatin1String(kEstimateLabelName));
  estimate_->setWordWrap(true);
  layout->addWidget(estimate_);

  problem_ = new QLabel(this);
  problem_->setObjectName(QLatin1String(kProblemLabelName));
  problem_->setWordWrap(true);
  problem_->setForegroundRole(QPalette::PlaceholderText);
  layout->addWidget(problem_);

  const auto refresh = [this] { Refresh(); };
  connect(whole_side_, &QRadioButton::toggled, this, refresh);
  connect(range_, &QRadioButton::toggled, this, refresh);
  connect(from_spin_up_, &QRadioButton::toggled, this, refresh);
  connect(key_lock_, &QCheckBox::toggled, this, refresh);

  if (start_frame_ != nullptr) {
    connect(start_frame_, &QSpinBox::valueChanged, this, refresh);
    connect(end_frame_, &QSpinBox::valueChanged, this, refresh);
  } else {
    connect(start_time_, &QLineEdit::textChanged, this, refresh);
    connect(end_time_, &QLineEdit::textChanged, this, refresh);
  }

  if (standard_ != nullptr) {
    connect(standard_, &QComboBox::currentIndexChanged, this, [this](int) {
      const auto chosen =
          static_cast<player::VideoStandard>(standard_->currentData().toInt());

      // Recorded as declared, which is what that provenance is for: the report
      // and the file then say the standard was stated rather than measured.
      if (chosen == player::VideoStandard::kUnknown) {
        disc_.video_standard = player::Fact<player::VideoStandard>{};
      } else {
        disc_.video_standard.Record(chosen, player::Provenance::kDeclared);
      }
      Refresh();
    });
  }

  ApplyShape();
  Refresh();
}

void CapturePlanForm::BuildAddressControls(QWidget* page, QFormLayout* form) {
  const int32_t first = ProgrammeStart(disc_);
  const int32_t last = ProgrammeEnd(disc_, clv_);

  if (clv_) {
    // Time codes, and no frame controls exist at all. Not disabled — absent:
    // a CLV disc has no frame numbers, and a greyed-out field for one invites
    // somebody to look for the setting that would turn it on.
    start_time_ = new QLineEdit(page);
    start_time_->setObjectName(QLatin1String(kStartTimeEditName));
    start_time_->setText(FormatTimeCode(first));
    start_time_->setPlaceholderText(tr("h:mm:ss"));
    form->addRow(tr("From"), start_time_);

    end_time_ = new QLineEdit(page);
    end_time_->setObjectName(QLatin1String(kEndTimeEditName));
    end_time_->setText(FormatTimeCode(last));
    end_time_->setPlaceholderText(tr("h:mm:ss"));
    form->addRow(tr("To"), end_time_);
    return;
  }

  start_frame_ = new QSpinBox(page);
  start_frame_->setObjectName(QLatin1String(kStartFrameSpinName));

  // The bounds are the measured ones, which is what makes a range that cannot
  // exist impossible to type rather than refused after the disc has started
  // spinning.
  start_frame_->setRange(first, last);
  start_frame_->setValue(first);
  form->addRow(tr("From frame"), start_frame_);

  end_frame_ = new QSpinBox(page);
  end_frame_->setObjectName(QLatin1String(kEndFrameSpinName));
  end_frame_->setRange(first, last);
  end_frame_->setValue(last);
  form->addRow(tr("To frame"), end_frame_);
}

int32_t CapturePlanForm::StartAddress() const {
  if (start_frame_ != nullptr) {
    return start_frame_->value();
  }
  const std::optional<int32_t> parsed = ParseTimeCodeEntry(start_time_->text());
  return parsed.value_or(-1);
}

int32_t CapturePlanForm::EndAddress() const {
  if (end_frame_ != nullptr) {
    return end_frame_->value();
  }
  const std::optional<int32_t> parsed = ParseTimeCodeEntry(end_time_->text());
  return parsed.value_or(-1);
}

player::AutoCapturePlan CapturePlanForm::Plan() const {
  player::AutoCapturePlan plan;

  plan.shape = range_->isChecked()          ? player::CaptureShape::kRange
               : from_spin_up_->isChecked() ? player::CaptureShape::kFromSpinUp
                                            : player::CaptureShape::kWholeSide;

  plan.addressing =
      clv_ ? player::AddressMode::kTimeCode : player::AddressMode::kFrame;

  // The two shapes that begin with a spin-up always start where the programme
  // does: the player is started from a stop and arrives there by itself, so a
  // start address the user typed would describe nothing.
  plan.start_address = plan.shape == player::CaptureShape::kRange
                           ? StartAddress()
                           : ProgrammeStart(disc_);
  plan.end_address = plan.shape == player::CaptureShape::kWholeSide
                         ? ProgrammeEnd(disc_, clv_)
                         : EndAddress();

  plan.key_lock = key_lock_->isChecked();
  return plan;
}

player::PlanProblem CapturePlanForm::problem() const {
  return ValidateAutoCapturePlan(Plan(), disc_);
}

bool CapturePlanForm::stop_capture_with_player() const {
  return stop_capture_->isChecked();
}

void CapturePlanForm::SetStopCaptureWithPlayer(bool on) {
  stop_capture_->setChecked(on);
}

void CapturePlanForm::SetEditable(bool editable) {
  editable_ = editable;
  ApplyShape();
}

void CapturePlanForm::ApplyShape() {
  // See the header: every control's state is set here from scratch rather than
  // being turned off when a capture starts and left to somebody to turn back
  // on.
  const bool editable = editable_;
  const bool ranged = range_->isChecked();
  const bool bounded = ranged || from_spin_up_->isChecked();

  const auto enable = [](QWidget* widget, bool on) {
    if (widget != nullptr) {
      widget->setEnabled(on);
    }
  };

  enable(whole_side_, editable);
  enable(from_spin_up_, editable);
  enable(range_, editable);

  enable(start_frame_, editable && ranged);
  enable(start_time_, editable && ranged);
  enable(end_frame_, editable && bounded);
  enable(end_time_, editable && bounded);

  enable(standard_, editable);
  enable(key_lock_, editable);
  enable(stop_capture_, editable);
}

QString CapturePlanForm::EstimateText() const {
  const CaptureSettings settings = LoadCaptureSettings();
  const double bytes_per_second = settings.EstimatedBytesPerSecond();

  const QString estimate = AutoCaptureEstimate(Plan(), disc_, bytes_per_second);
  if (estimate.isEmpty()) {
    return tr(
        "How long this will take is not known: a frame count is only a "
        "duration once the video standard is known.");
  }

  const std::optional<std::chrono::seconds> duration =
      player::PlannedDuration(Plan(), disc_);

  const capture::FreeSpace space = capture::AvailableSpace(
      std::filesystem::path(settings.ResolvedCaptureDirectory().toStdString()));

  if (!space.known || !duration.has_value() || bytes_per_second <= 0.0) {
    return estimate;
  }

  const auto needed = static_cast<uint64_t>(
      static_cast<double>(duration->count()) * bytes_per_second);

  if (needed > space.bytes_available) {
    // Said before the disc starts spinning rather than by the low-space warning
    // forty minutes in — which is the whole reason the estimate is here.
    return tr("%1 There is only %2 free where captures are written, so this "
              "one will not fit.")
        .arg(estimate, FormatByteSize(space.bytes_available));
  }

  return tr("%1 %2 free where captures are written.")
      .arg(estimate, FormatByteSize(space.bytes_available));
}

void CapturePlanForm::Refresh() {
  ApplyShape();

  estimate_->setText(EstimateText());
  problem_->setText(PlanProblemText(problem()));

  emit Changed();
}

}  // namespace ddd::gui
