#pragma once

#include <QWidget>

#include "Levers.h"

class QListWidget;
class QLineEdit;
class QTextBrowser;

class SwitcherPage : public QWidget {
  Q_OBJECT

 public:
  SwitcherPage(QWidget* parent = nullptr);
  ~SwitcherPage() override;

  void load();
  // 返回是否有改动被写入
  bool save();

 private:
  void populate();
  void showDetails(RimeSchemaInfo* info);
  void getMoreSchemas();

  RimeLeversApi* api_ = nullptr;
  RimeSwitcherSettings* settings_ = nullptr;
  RimeSchemaList available_ = {0};

  QListWidget* schemaList_ = nullptr;
  QTextBrowser* description_ = nullptr;
  QLineEdit* hotkeys_ = nullptr;
  bool modified_ = false;
};
