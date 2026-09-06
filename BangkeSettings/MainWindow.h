#pragma once

#include <QMainWindow>

class Configurator;
class QListWidget;
class QStackedWidget;
class SwitcherPage;
class StylePage;
class DictPage;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow(Configurator* configurator, bool openDictPage = false,
             QWidget* parent = nullptr);
  ~MainWindow() override;

 protected:
  void closeEvent(QCloseEvent* event) override;

 private slots:
  void onPageChanged(int index);
  void saveAndDeploy();

 private:
  Configurator* configurator_ = nullptr;
  QListWidget* nav_ = nullptr;
  QStackedWidget* stack_ = nullptr;
  SwitcherPage* switcherPage_ = nullptr;
  StylePage* stylePage_ = nullptr;
  DictPage* dictPage_ = nullptr;
};
