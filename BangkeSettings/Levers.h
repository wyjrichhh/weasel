#pragma once

#include <rime_api.h>
#include <rime_levers_api.h>

inline RimeLeversApi* leversApi() {
  RimeModule* module = rime_get_api()->find_module("levers");
  return module ? (RimeLeversApi*)module->get_api() : nullptr;
}
