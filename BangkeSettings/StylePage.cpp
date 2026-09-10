#include "StylePage.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPainter>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Ui.h"

// 迷你候选窗预览:按 scheme 原始颜色直接绘制,与真面板同数据源
// (共享 weasel.yaml),不再依赖 preview\*.png
class SchemePreviewWidget : public QWidget {
 public:
  SchemePreviewWidget(QWidget* parent = nullptr) : QWidget(parent) {
    setMinimumSize(380, 180);
  }

  void setScheme(const std::map<std::string, unsigned int>& colors,
                 bool horizontal) {
    colors_ = colors;
    horizontal_ = horizontal;
    update();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    auto color = [this](const char* key, QRgb def) {
      auto it = colors_.find(key);
      return it == colors_.end() ? def : (QRgb)it->second;
    };
    const QColor back(color("back_color", 0xFFF2FFFFFF));
    const QColor border(color("border_color", 0xFFE3E3E8));
    const QColor shadow(color("shadow_color", 0x28000000));
    const QColor text(color("text_color", 0xFF1F2937));
    const QColor candText(color("candidate_text_color", 0xFF2F3437));
    const QColor label(color("label_color", 0xFF8C8C8C));
    const QColor hiliteText(
        color("hilited_candidate_text_color", 0xFFFFFFFF));
    const QColor hiliteBack(
        color("hilited_candidate_back_color", 0xFF0D9488));
    const QColor hiliteLabel(color("hilited_label_color", 0xD9FFFFFF));

    // 阴影余量 + 面板矩形(圆角 10,与真面板默认一致)
    const QRectF panel = rect().adjusted(16, 10, -16, -16);
    p.setPen(Qt::NoPen);
    for (int i = 3; i >= 1; --i) {
      QColor s = shadow;
      s.setAlpha(s.alpha() / (4 - i));
      p.setBrush(s);
      p.drawRoundedRect(panel.translated(0, i * 2).adjusted(-1, -1, 1, 1),
                        11.0 + i, 11.0 + i);
    }
    p.setBrush(back);
    p.setPen(QPen(border, 1));
    p.drawRoundedRect(panel, 10, 10);
    p.setPen(Qt::NoPen);

    QFont f = font();
    f.setFamily(QStringLiteral("Microsoft YaHei UI"));
    f.setPointSize(9);
    p.setFont(f);

    const Cand cands[] = {
        {QStringLiteral("1."), QStringLiteral(u"蚌壳"), QStringLiteral(u"预测")},
        {QStringLiteral("2."), QStringLiteral(u"帮客"), QString()},
        {QStringLiteral("3."), QStringLiteral(u"绑客"), QString()},
    };

    p.setPen(text);
    p.drawText(QRectF(panel.left() + 12, panel.top() + 8,
                      panel.width() - 24, 20),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral(u"bang'ke pinyin"));

    if (horizontal_) {
      double x = panel.left() + 10;
      for (int i = 0; i < 3; ++i) {
        const double w = 74 + (cands[i].comment.isEmpty() ? 0 : 46);
        drawCandidate(p, QRectF(x, panel.top() + 32, w, 30), cands[i], i == 0,
                      label, candText, hiliteLabel, hiliteText, hiliteBack);
        x += w + 8;
      }
    } else {
      double y = panel.top() + 30;
      for (int i = 0; i < 3; ++i) {
        drawCandidate(p, QRectF(panel.left() + 8, y, panel.width() - 16, 30),
                      cands[i], i == 0, label, candText, hiliteLabel,
                      hiliteText, hiliteBack);
        y += 34;
      }
    }
  }

 private:
  struct Cand {
    QString label, text, comment;
  };

  void drawCandidate(QPainter& p, const QRectF& box, const Cand& c,
                     bool hilited, const QColor& label, const QColor& text,
                     const QColor& hiliteLabel, const QColor& hiliteText,
                     const QColor& hiliteBack) {
    if (hilited) {
      p.setPen(Qt::NoPen);
      p.setBrush(hiliteBack);
      p.drawRoundedRect(box, 6, 6);
    }
    p.setPen(hilited ? hiliteLabel : label);
    p.drawText(QRectF(box.left() + 8, box.top(), 22, box.height()),
               Qt::AlignLeft | Qt::AlignVCenter, c.label);
    p.setPen(hilited ? hiliteText : text);
    p.drawText(QRectF(box.left() + 30, box.top(), 50, box.height()),
               Qt::AlignLeft | Qt::AlignVCenter, c.text);
    if (!c.comment.isEmpty()) {
      p.setPen(hilited ? hiliteLabel : label);
      p.drawText(QRectF(box.left() + 82, box.top(), 60, box.height()),
                 Qt::AlignLeft | Qt::AlignVCenter, c.comment);
    }
  }

  std::map<std::string, unsigned int> colors_;
  bool horizontal_ = false;
};

StylePage::StylePage(QWidget* parent) : QWidget(parent) {
  schemeCombo_ = new QComboBox(this);
  fontSize_ = new QSpinBox(this);
  fontSize_->setRange(9, 36);

  layoutCombo_ = new QComboBox(this);
  layoutCombo_->addItem(QStringLiteral(u"竖排"), QStringLiteral("vertical"));
  layoutCombo_->addItem(QStringLiteral(u"横排"), QStringLiteral("horizontal"));

  auto* form = new QFormLayout();
  form->setSpacing(8);
  form->addRow(QStringLiteral(u"配色方案："), schemeCombo_);
  form->addRow(QStringLiteral(u"字体大小："), fontSize_);
  form->addRow(QStringLiteral(u"候选窗排列："), layoutCombo_);

  preview_ = new SchemePreviewWidget(this);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);
  layout->addWidget(makeCard(QStringLiteral(u"外观"), form));
  auto* prevForm = new QVBoxLayout();
  prevForm->addWidget(preview_);
  layout->addWidget(makeCard(QStringLiteral(u"预览"), prevForm));
  layout->addStretch(1);

  connect(schemeCombo_, &QComboBox::currentIndexChanged, this,
          &StylePage::updatePreview);
  connect(layoutCombo_, &QComboBox::currentIndexChanged, this,
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
  activeFontSize_ = settings_.GetFontSize(14);
  fontSize_->setValue(activeFontSize_);
  activeHorizontal_ = settings_.GetHorizontal(false);
  layoutCombo_->setCurrentIndex(activeHorizontal_ ? 1 : 0);
  updatePreview();
  loaded_ = true;
}

void StylePage::updatePreview() {
  const QString id = schemeCombo_->currentData().toString();
  if (id.isEmpty())
    return;
  const bool horizontal =
      layoutCombo_->currentData().toString() == QStringLiteral("horizontal");
  preview_->setScheme(settings_.GetSchemeColors(id.toStdString()),
                      horizontal);
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
  const bool horizontal =
      layoutCombo_->currentData().toString() == QStringLiteral("horizontal");
  if (horizontal != activeHorizontal_) {
    settings_.SetHorizontal(horizontal);
    changed = true;
  }
  if (!changed)
    return false;
  return settings_.Save();
}
