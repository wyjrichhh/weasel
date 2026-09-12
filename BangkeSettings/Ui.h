#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QString>
#include <QTimer>
#include <QWidget>

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
