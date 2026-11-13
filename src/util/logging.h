// Thin wrapper over ESP_LOG so call sites don't need to care whether we're building for
// the native_test env (where ESP_LOG isn't available) or real hardware.
#pragma once

#ifdef UNIT_TEST
#include <cstdio>
namespace util {
inline void LogInfo(const char* tag, const char* msg) { std::printf("[I][%s] %s\n", tag, msg); }
inline void LogWarn(const char* tag, const char* msg) { std::printf("[W][%s] %s\n", tag, msg); }
inline void LogError(const char* tag, const char* msg) { std::printf("[E][%s] %s\n", tag, msg); }
} // namespace util
#else
#include <esp_log.h>
namespace util {
inline void LogInfo(const char* tag, const char* msg) { ESP_LOGI(tag, "%s", msg); }
inline void LogWarn(const char* tag, const char* msg) { ESP_LOGW(tag, "%s", msg); }
inline void LogError(const char* tag, const char* msg) { ESP_LOGE(tag, "%s", msg); }
} // namespace util
#endif
