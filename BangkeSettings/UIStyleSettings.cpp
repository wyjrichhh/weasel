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

// 归一为 0xAARRGGBB。6 位值补 0xff alpha,8 位按声明的 color_format 换序
// (与 server 端 _RimeGetColor 的语义一致)
static unsigned int NormalizeColor(unsigned int v,
                                   const std::string& format,
                                   bool has_alpha) {
  if (!has_alpha)
    v |= 0xFF000000;
  if (format == "argb")
    return v;
  if (format == "rgba")
    return ((v & 0x000000FF) << 24) | (v & 0xFFFFFF00);
  return (v & 0xFF000000) | ((v & 0x000000FF) << 16) | (v & 0x0000FF00) |
         ((v & 0x00FF0000) >> 16);
}

std::map<std::string, unsigned int> UIStyleSettings::GetSchemeColors(
    const std::string& color_scheme_id) {
  std::map<std::string, unsigned int> result;
  std::ifstream ifs(wtou8(WeaselSharedDataPath().wstring()) + "\\weasel.yaml");
  if (!ifs.good())
    return result;
  std::string line;
  bool inSchemes = false, inScheme = false;
  std::string format = "abgr";
  const std::string want = "  " + color_scheme_id + ":";
  while (std::getline(ifs, line)) {
    if (line.find("preset_color_schemes:") != std::string::npos) {
      inSchemes = true;
      continue;
    }
    if (!inSchemes)
      continue;
    if (!line.empty() && line[0] != ' ' && line[0] != '#')
      break;  // 退出块
    if (line.rfind(want, 0) == 0) {
      inScheme = true;
      continue;
    }
    if (inScheme && line.size() > 2 && line[0] == ' ' && line[1] == ' ' &&
        line[2] != ' ') {
      break;  // 下一个方案开始
    }
    if (!inScheme)
      continue;
    const auto colon = line.find(':');
    if (colon == std::string::npos)
      continue;
    std::string key = line.substr(4, colon - 4);
    std::string val = line.substr(colon + 1);
    val.erase(0, val.find_first_not_of(" \t"));
    if (key == "color_format") {
      val.erase(val.find_last_not_of(" \t\r\n#") + 1);
      format = val;
      continue;
    }
    if (key == "name" || key == "author")
      continue;
    // 只收颜色键:剥注释后 0x.. / 十进制
    const auto hash = val.find('#');
    if (hash != std::string::npos)
      val = val.substr(0, hash);
    val.erase(val.find_last_not_of(" \t\r\n") + 1);
    if (val.empty())
      continue;
    try {
      // 0x 前缀按 16 进制;带 0x 共 10 字符(=8 位数字)视为带 alpha
      const bool hex = val.rfind("0x", 0) == 0 || val.rfind("0X", 0) == 0;
      unsigned int v = hex ? std::stoul(val, nullptr, 16) : std::stoul(val);
      result[key] = NormalizeColor(
          v, format, hex ? val.length() > 8 : val.length() >= 10);
    } catch (...) {
    }
  }
  return result;
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
  if (!rime_get_api()->config_get_int(&config, "style/font_point", &value))
    return fallback;
  return value;
}

void UIStyleSettings::SetFontSize(int value) {
  api_->customize_int(settings_, "style/font_point", value);
}

bool UIStyleSettings::GetHorizontal(bool fallback) {
  RimeConfig config = {0};
  if (!api_->settings_get_config(settings_, &config))
    return fallback;
  Bool value = fallback;
  if (!rime_get_api()->config_get_bool(&config, "style/horizontal", &value))
    return fallback;
  return value != False;
}

void UIStyleSettings::SetHorizontal(bool value) {
  api_->customize_bool(settings_, "style/horizontal", value);
}

// 共享 weasel.yaml 的 style:→layout: 块整表(显示"当前生效值"的默认基线)
static std::map<std::string, int> ScanSharedLayout() {
  std::map<std::string, int> result;
  std::ifstream ifs(wtou8(WeaselSharedDataPath().wstring()) + "\\weasel.yaml");
  if (!ifs.good())
    return result;
  std::string line;
  bool inStyle = false, inLayout = false;
  while (std::getline(ifs, line)) {
    if (line.rfind("style:", 0) == 0) {
      inStyle = true;
      continue;
    }
    if (inStyle && !line.empty() && line[0] != ' ' && line[0] != '#')
      inStyle = false;
    if (!inStyle)
      continue;
    if (line.rfind("  layout:", 0) == 0) {
      inLayout = true;
      continue;
    }
    if (inLayout && line.size() > 4 && line[0] == ' ' && line[1] == ' ' &&
        line[2] == ' ' && line[3] == ' ') {
      const auto colon = line.find(':');
      if (colon != std::string::npos) {
        std::string key = line.substr(4, colon - 4);
        std::string val = line.substr(colon + 1);
        const auto hash = val.find('#');
        if (hash != std::string::npos)
          val = val.substr(0, hash);
        val.erase(val.find_last_not_of(" \t\r\n") + 1);
        try {
          result[key] = val.empty() ? 0 : std::stoi(val);
        } catch (...) {
        }
      }
      continue;
    }
    if (inLayout)
      break;  // layout 块结束(回到 2 空格键)
  }
  return result;
}

int UIStyleSettings::GetLayoutInt(const std::string& key, int fallback) {
  // 补丁优先;未打补丁则用共享默认;再兜底代码默认
  RimeConfig config = {0};
  if (api_->settings_get_config(settings_, &config)) {
    int v = 0;
    if (rime_get_api()->config_get_int(
            &config, ("style/layout/" + key).c_str(), &v))
      return v;
  }
  const auto shared = ScanSharedLayout();
  auto it = shared.find(key);
  return it != shared.end() ? it->second : fallback;
}

void UIStyleSettings::SetLayoutInt(const std::string& key, int value) {
  api_->customize_int(settings_, ("style/layout/" + key).c_str(), value);
}

bool UIStyleSettings::HasCustomScheme() {
  RimeConfig config = {0};
  if (!api_->settings_get_config(settings_, &config))
    return false;
  return rime_get_api()->config_get_cstring(
             &config, "preset_color_schemes/bangke_custom/back_color") !=
         nullptr;
}

unsigned int UIStyleSettings::GetCustomColor(const std::string& key,
                                             unsigned int fallback) {
  RimeConfig config = {0};
  if (!api_->settings_get_config(settings_, &config))
    return fallback;
  int v = 0;
  if (!rime_get_api()->config_get_int(
          &config, ("preset_color_schemes/bangke_custom/" + key).c_str(), &v))
    return fallback;
  return (unsigned int)v;
}

void UIStyleSettings::SetCustomColor(const std::string& key,
                                     unsigned int argb) {
  api_->customize_int(settings_,
                      ("preset_color_schemes/bangke_custom/" + key).c_str(),
                      (int)argb);
  // 解析端按方案节点声明的 color_format 换序;一次性声明为 argb,
  // 这里写入的 0xAARRGGBB 即最终语义
  api_->customize_string(settings_, "preset_color_schemes/bangke_custom/name",
                         "自定义");
  api_->customize_string(settings_,
                         "preset_color_schemes/bangke_custom/color_format",
                         "argb");
}
