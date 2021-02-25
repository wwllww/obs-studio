#ifndef WEBRTC_CORE_H
#define WEBRTC_CORE_H
#include <obs-module.h>
#include <curl/curl.h>
#include <webrtc-core/webrtc_core_interface.h>
#include "rtc-socket-interface.h"

#define log_warn(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)
#define log_info(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define log_debug(format, ...) blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define log_error(format, ...) blog(LOG_ERROR, format, ##__VA_ARGS__)

#endif // !WEBRTC_CORE_H
