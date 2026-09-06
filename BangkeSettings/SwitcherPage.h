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

  // 首次进入页面时加载；切页往返不重置未保存的勾选
  void load();
  // 丢弃当前列表强制重读（刷新 / 部署后同步）
  void forceLoad();
  // 返回是否有改动被写入
  bool save();

 private:
  void loadSettings();
  void populate();
  void showDetails(RimeSchemaInfo* info);
  void getMoreSchemas();

  RimeLeversApi* api_ = nullptr;
  RimeSwitcherSettings* settings_ = nullptr;
  RimeSchemaList available_ = {0};

  QListWidget* schemaList_ = nullptr;
  QTextBrowser* description_ = nullptr;
  QLineEdit* hotkeys_ = nullptr;
  bool loaded_ = false;
  bool modified_ = false;
};
