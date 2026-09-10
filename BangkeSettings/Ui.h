#pragma once

#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QString>

// 卡片容器:QFrame#card + 可选标题;底色/圆角/边框由 Theme 的 QSS 提供
inline QFrame* makeCard(const QString& title, QLayout* inner,
                        QWidget* parent = nullptr) {
  auto* f = new QFrame(parent);
  f->setObjectName(QStringLiteral("card"));
  auto* v = new QVBoxLayout(f);
  v->setContentsMargins(16, 12, 16, 16);
  v->setSpacing(10);
  if (!title.isEmpty()) {
    auto* t = new QLabel(title, f);
    t->setObjectName(QStringLiteral("cardTitle"));
    v->addWidget(t);
  }
  v->addLayout(inner);
  return f;
}
