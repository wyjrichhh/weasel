#pragma once

#include <QWidget>

#include <functional>
#include <vector>

#include "Levers.h"

class QLineEdit;
class QTextBrowser;
class ToggleSwitch;

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
  void addRow(RimeSchemaListItem& item, RimeSchemaInfo* info, bool checked, size_t index);
  void populate();
  void showDetails(RimeSchemaInfo* info);

  RimeLeversApi* api_ = nullptr;
  RimeSwitcherSettings* settings_ = nullptr;
  RimeSchemaList available_ = {0};

  struct SchemaRowRec {
    RimeSchemaInfo* info = nullptr;
    ToggleSwitch* sw = nullptr;
    QWidget* row = nullptr;
  };
  QWidget* listHost_ = nullptr;
  std::vector<SchemaRowRec> rows_;
  QTextBrowser* description_ = nullptr;
  QLineEdit* hotkeys_ = nullptr;
  bool loaded_ = false;
  bool modified_ = false;
};
