#include "StylePage.h"

#include <QBoxLayout>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>

StylePage::StylePage(QWidget* parent) : QWidget(parent) {
  schemeCombo_ = new QComboBox(this);
  preview_ = new QLabel(this);
  preview_->setMinimumSize(320, 160);
  preview_->setAlignment(Qt::AlignCenter);
  fontSize_ = new QSpinBox(this);
  fontSize_->setRange(9, 36);

  auto* form = new QFormLayout();
  form->addRow(L"配色方案：", schemeCombo_);
  form->addRow(L"字体大小：", fontSize_);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addWidget(new QLabel(L"预览：", this));
  layout->addWidget(preview_, 0, Qt::AlignLeft | Qt::AlignTop);
  layout->addStretch();

  connect(schemeCombo_, &QComboBox::currentIndexChanged, this,
          &StylePage::updatePreview);
}

void StylePage::load() {
  if (loaded_)
    return;
  forceLoad();
}

void StylePage::forceLoad() {
  settings_.Load();

  std::vector<ColorSchemeInfo> schemes;
  settings_.GetPresetColorSchemes(&schemes);
  std::string active = settings_.GetActiveColorScheme();

  schemeCombo_->blockSignals(true);
  schemeCombo_->clear();
  for (auto& s : schemes)
    schemeCombo_->addItem(QString::fromStdString(s.name),
                          QString::fromStdString(s.color_scheme_id));
  schemeCombo_->blockSignals(false);

  int index = schemeCombo_->findData(QString::fromStdString(active));
  schemeCombo_->setCurrentIndex(index >= 0 ? index : 0);

  activeScheme_ = settings_.GetActiveColorScheme();
  activeFontSize_ = settings_.GetFontSize(15);
  fontSize_->setValue(activeFontSize_);
  updatePreview();
  loaded_ = true;
}

void StylePage::updatePreview() {
  QString id = schemeCombo_->currentData().toString();
  if (id.isEmpty())
    return;
  QString file =
      QString::fromStdString(settings_.GetColorSchemePreview(id.toStdString()));
  QPixmap pixmap(file);
  if (pixmap.isNull())
    preview_->setText(L"（无预览图）");
  else
    preview_->setPixmap(pixmap.scaled(preview_->size() * 0.9,
                                      Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation));
}

bool StylePage::save() {
  bool changed = false;
  std::string scheme = schemeCombo_->currentData().toString().toStdString();
  if (!scheme.empty() && scheme != activeScheme_) {
    settings_.SelectColorScheme(scheme);
    changed = true;
  }
  if (fontSize_->value() != activeFontSize_) {
    settings_.SetFontSize(fontSize_->value());
    changed = true;
  }
  if (!changed)
    return false;
  return settings_.Save();
}
