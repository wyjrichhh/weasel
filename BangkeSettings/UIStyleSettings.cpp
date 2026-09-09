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
  // 直接读共享 weasel.yaml,行扫描 preset_color_schemes: 块下的 name:
  // (不走 rime API——config_load_string 对大 yaml 不稳定,行扫描更简单可靠)
  std::string shared = rime_get_api()->get_shared_data_dir();
  std::string path = shared + "\\weasel.yaml";
  std::ifstream ifs(wtou8(WeaselSharedDataPath().wstring()) + "\\weasel.yaml");
  if (!ifs.good())
    return false;
  std::string line;
  bool inBlock = false;
  while (std::getline(ifs, line)) {
    if (line.find("preset_color_schemes:") != std::string::npos) {
      inBlock = true;
      continue;
    }
    if (inBlock) {
      // 块内条目形如 "  <id>:" 后跟 "    name: <名称>"
      if (!line.empty() && line[0] != ' ' && line[0] != '#')
        break;  // 退出块
      // 找 "    name: xxx" 行
      auto namePos = line.find("name:");
      if (namePos != std::string::npos && namePos < 8) {
        std::string name = line.substr(namePos + 5);
        // trim
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t\r\n") + 1);
        if (!name.empty()) {
          // 找上一个非 name 行作为 id(形如 "  <id>:")
          result->push_back(ColorSchemeInfo());
          result->back().name = name;
          // id 取 name 的下划线形式(展示用;实际选择用 name 也可)
          result->back().color_scheme_id = name;
        }
      }
    }
  }
  ifs.close();
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
