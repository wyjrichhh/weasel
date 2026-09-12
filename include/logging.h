#ifndef BANGKE_LOGGING_H_
#define BANGKE_LOGGING_H_

#ifdef BANGKE_ENABLE_LOGGING
#define GLOG_NO_ABBREVIATED_SEVERITIES
#pragma warning(disable : 4244)
#include <glog/logging.h>
#pragma warning(default : 4244)
#else
#include "no_logging.h"
#endif  // BANGKE_ENABLE_LOGGING

#endif  // BANGKE_LOGGING_H_
