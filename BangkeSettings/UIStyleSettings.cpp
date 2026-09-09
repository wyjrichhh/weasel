#include "UIStyleSettings.h"

#include <windows.h>

#include <fstream>
#include <string>

#include "Levers.h"
#include <WeaselUtility.h>
#include <rime_api.h>

UIStyleSettings::UIStyleSettings() {
  api_ = leversApi();
  settings_ = api_->custom_settings_init("weasel", "Bangke::UIStyleSettings");
}

bool UIStyleSettings::Load() {
  return api_->load_settings(settings_);
}

bool UIStyleSettings::Save() {
  return api_->save_settings(settings_);
}

bool UIStyleSettings::GetPresetColorSchemes(
    std::vector<ColorSchemeInfo>* result) {
  if (!result)
    return false;
  result->clear();
  // 直接读共享 weasel.yaml,行扫描 preset_color_schemes: 块:
  // 二空格 "  <id>:" 条目行与随后的 "    name: <名称>" 配对。
  // 不走 rime API——config_load_string 对大 yaml 不稳定。
  std::ifstream ifs(wtou8(WeaselSharedDataPath().wstring()) + "\\weasel.yaml");
  if (!ifs.good())
    return false;
  std::string line;
  bool inBlock = false;
  std::string pendingId;
  while (std::getline(ifs, line)) {
    if (line.find("preset_color_schemes:") != std::string::npos) {
      inBlock = true;
      continue;
    }
    if (!inBlock)
      continue;
    if (!line.empty() && line[0] != ' ' && line[0] != '#')
      break;  // 退出块
    if (line.size() > 2 && line[0] == ' ' && line[1] == ' ' && line[2] != ' ' &&
        line[2] != '#') {
      const auto colon = line.find(':');
      if (colon != std::string::npos) {
        pendingId = line.substr(2, colon - 2);
        continue;
      }
    }
    auto namePos = line.find("name:");
    if (namePos != std::string::npos && namePos < 8 && !pendingId.empty()) {
      std::string name = line.substr(namePos + 5);
      name.erase(0, name.find_first_not_of(" \t"));
      name.erase(name.find_last_not_of(" \t\r\n") + 1);
      if (!name.empty()) {
        result->push_back(ColorSchemeInfo());
        result->back().name = name;
        result->back().color_scheme_id = pendingId;
        pendingId.clear();
      }
    }
  }
  return !result->empty();
}

static inline bool IfFileExist(std::string filename) {
  DWORD dwAttrib = GetFileAttributes(acptow(filename).c_str());
  return (INVALID_FILE_ATTRIBUTES != dwAttrib &&
          0 == (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

std::string UIStyleSettings::GetColorSchemePreview(
    const std::string& color_scheme_id) {
  std::string shared_dir = rime_get_api()->get_shared_data_dir();
  std::string user_dir = rime_get_api()->get_user_data_dir();
  std::string filename =
      user_dir + "\\preview\\color_scheme_" + color_scheme_id + ".png";
  if (IfFileExist(filename))
    return filename;
  return (shared_dir + "\\preview\\color_scheme_" + color_scheme_id + ".png");
}

std::string UIStyleSettings::GetActiveColorScheme() {
  RimeConfig config = {0};
  api_->settings_get_config(settings_, &config);
  const char* value =
      rime_get_api()->config_get_cstring(&config, "style/color_scheme");
  if (!value)
    return std::string();
  return std::string(value);
}

bool UIStyleSettings::SelectColorScheme(const std::string& color_scheme_id) {
  api_->customize_string(settings_, "style/color_scheme",
                         color_scheme_id.c_str());
  return true;
}

int UIStyleSettings::GetFontSize(int fallback) {
  RimeConfig config = {0};
  if (!api_->settings_get_config(settings_, &config))
    return fallback;
  int value = fallback;
  if (!rime_get_api()->config_get_int(&config, "style/font_size", &value))
    return fallback;
  return value;
}

void UIStyleSettings::SetFontSize(int value) {
  api_->customize_int(settings_, "style/font_size", value);
}
