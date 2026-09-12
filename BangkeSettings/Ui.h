#pragma once

#include <QAbstractButton>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QPainter>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

// 微信式滑动开关:药丸轨道 + 滑块,替代 QCheckBox
class ToggleSwitch : public QAbstractButton {
 public:
  explicit ToggleSwitch(QWidget* parent = nullptr) : QAbstractButton(parent) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(46, 24);
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    p.setPen(Qt::NoPen);
    p.setBrush(isChecked() ? QColor(0x0D, 0x94, 0x88)
                           : (dark ? QColor(0x3A, 0x3A, 0x42)
                                   : QColor(0xC9, 0xCD, 0xD4)));
    p.drawRoundedRect(rect(), height() / 2.0, height() / 2.0);
    const int d = height() - 6;
    const int x = isChecked() ? width() - d - 3 : 3;
    p.setBrush(QColor(0xFF, 0xFF, 0xFF));
    p.drawEllipse(x, 3, d, d);
  }
};

// 卡片容器:QFrame#card + 可选标题;底色/圆角/边框由 Theme 的 QSS 提供
inline QFrame* makeCard(const QString& title, QLayout* inner,
                        QWidget* parent = nullptr) {
  auto* f = new QFrame(parent);
  f->setObjectName(QStringLiteral("card"));
  auto* v = new QVBoxLayout(f);
  v->setContentsMargins(14, 10, 14, 12);
  v->setSpacing(8);
  if (!title.isEmpty()) {
    auto* t = new QLabel(title, f);
    t->setObjectName(QStringLiteral("cardTitle"));
    v->addWidget(t);
  }
  v->addLayout(inner);
  return f;
}

// 微信式设置行:标题(+灰色副文案)居左,控件贴右缘垂直居中
inline QWidget* makeSettingRow(const QString& title, QWidget* control,
                               const QString& helper = QString()) {
  auto* row = new QWidget;
  auto* h = new QHBoxLayout(row);
  h->setContentsMargins(0, 9, 0, 9);
  h->setSpacing(12);
  auto* text = new QVBoxLayout();
  text->setSpacing(2);
  auto* t = new QLabel(title);
  t->setObjectName(QStringLiteral("rowTitle"));
  text->addWidget(t);
  if (!helper.isEmpty()) {
    auto* sub = new QLabel(helper);
    sub->setObjectName(QStringLiteral("rowHint"));
    text->addWidget(sub);
  }
  h->addLayout(text);
  h->addStretch(1);
  if (control)
    h->addWidget(control, 0, Qt::AlignRight | Qt::AlignVCenter);
  return row;
}

inline QFrame* makeHairline() {
  auto* line = new QFrame;
  line->setObjectName(QStringLiteral("line"));
  line->setFrameShape(QFrame::HLine);
  return line;
}

// 行组:上下排布,行间自动插发丝分隔线
inline QLayout* makeSettingRows(const std::vector<QWidget*>& rows) {
  auto* v = new QVBoxLayout();
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(0);
  for (size_t i = 0; i < rows.size(); ++i) {
    if (i)
      v->addWidget(makeHairline());
    v->addWidget(rows[i]);
  }
  return v;
}

// 非阻塞完成提示:父窗口内浮层,自动消散
inline void makeToast(const QString& text, QWidget* host) {
  auto* toast = new QLabel(text, host->window());
  toast->setObjectName(QStringLiteral("toast"));
  toast->setAlignment(Qt::AlignCenter);
  toast->adjustSize();
  auto* w = host->window();
  toast->move(w->width() / 2 - toast->width() / 2,
              w->height() - toast->height() - 56);
  toast->show();
  toast->raise();
  QTimer::singleShot(1800, toast, &QObject::deleteLater);
}
