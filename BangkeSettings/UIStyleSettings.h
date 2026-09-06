#pragma once

#include <rime_levers_api.h>
#include <string>
#include <vector>

struct ColorSchemeInfo {
  std::string color_scheme_id;
  std::string name;
  std::string author;
};

class UIStyleSettings {
 public:
  UIStyleSettings();

  bool Load();
  bool Save();

  bool GetPresetColorSchemes(std::vector<ColorSchemeInfo>* result);
  std::string GetColorSchemePreview(const std::string& color_scheme_id);
  std::string GetActiveColorScheme();
  bool SelectColorScheme(const std::string& color_scheme_id);

  int GetFontSize(int fallback);
  void SetFontSize(int value);

  RimeCustomSettings* settings() { return settings_; }

 private:
  RimeLeversApi* api_;
  RimeCustomSettings* settings_;
};
