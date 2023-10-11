#pragma once
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
void metrics_event_playback(const char* source);
void metrics_event_boot(const char* partition);
void metrics_event(const char* name);
void metrics_add_feature(const char* name, bool active);
void metrics_add_feature_variant(const char* name, const char* format, ...);
void metrics_init();
void metrics_flush();

#ifdef __cplusplus
}
#endif
