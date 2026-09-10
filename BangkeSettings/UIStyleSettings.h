#pragma once

#include <rime_levers_api.h>
#include <cstdint>
#include <map>
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
  // 指定方案的原始配色键值,统一归一为 0xAARRGGBB
  std::map<std::string, unsigned int> GetSchemeColors(
      const std::string& color_scheme_id);
  std::string GetActiveColorScheme();
  bool SelectColorScheme(const std::string& color_scheme_id);

  int GetFontSize(int fallback);
  void SetFontSize(int value);

  bool GetHorizontal(bool fallback);
  void SetHorizontal(bool value);

  RimeCustomSettings* settings() { return settings_; }

 private:
  RimeLeversApi* api_;
  RimeCustomSettings* settings_;
};
