#pragma once
#include "config.h"
#ifndef SERIAL_DIAGNOSTICS
#define SERIAL_DIAGNOSTICS 1
#endif
#ifndef AUDIO_DIAGNOSTIC
#define AUDIO_DIAGNOSTIC 0
#endif
#ifdef __cplusplus
extern "C" {
#endif
void disco_log(const char *level, const char *tag, const char *format, ...) __attribute__((format(printf,3,4)));
#ifdef __cplusplus
}
#endif
#if SERIAL_DIAGNOSTICS
#define DISCO_LOGI(tag, ...) disco_log("I", tag, __VA_ARGS__)
#define DISCO_LOGW(tag, ...) disco_log("W", tag, __VA_ARGS__)
#define DISCO_LOGE(tag, ...) disco_log("E", tag, __VA_ARGS__)
#else
#define DISCO_LOGI(...) ((void)0)
#define DISCO_LOGW(...) ((void)0)
#define DISCO_LOGE(...) ((void)0)
#endif
