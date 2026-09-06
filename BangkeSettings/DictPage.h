#pragma once

#include <QWidget>

#include "Levers.h"

class QListWidget;
class QPushButton;

class DictPage : public QWidget {
  Q_OBJECT

 public:
  DictPage(QWidget* parent = nullptr);

  void load();
  // 维护会话（互斥量 + server maintenance）由 MainWindow 切页时驱动
  void setSessionActive(bool active);

 private:
  void populate();
  void backup();
  void restore();
  void exportDict();
  void importDict();
  void updateButtons();

  RimeLeversApi* api_ = nullptr;
  QListWidget* dictList_ = nullptr;
  QPushButton* backupBtn_ = nullptr;
  QPushButton* restoreBtn_ = nullptr;
  QPushButton* exportBtn_ = nullptr;
  QPushButton* importBtn_ = nullptr;
  bool sessionActive_ = false;
};
