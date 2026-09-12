#pragma once

#define BANGKE_CODE_NAME "Bangke"
#define BANGKE_REG_KEY L"Software\\Bangke"
#define RIME_REG_KEY L"Software\\Rime"

#define STRINGIZE(x) #x
#define VERSION_STR(x) STRINGIZE(x)
#define BANGKE_VERSION VERSION_STR(VERSION_MAJOR.VERSION_MINOR.VERSION_PATCH)
