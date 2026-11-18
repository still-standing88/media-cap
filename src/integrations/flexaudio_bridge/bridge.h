#pragma once
#include "MediaCap_audio.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Private ABI. All handles are exclusively accessed on their owning capture worker.
 * Chunk/list owners can be freed on another thread; their data is immutable. */
typedef struct McFlexError {
    int32_t code;
    int64_t native_code;
    char message[512];
} McFlexError;
typedef struct McFlexChunk {
    const float *data;
    size_t length;
    uint32_t frames;
    uint32_t sample_rate;
    uint16_t channels;
    int64_t media_ns;
    int64_t arrival_ns; /* Ingest label relative to the stream's private epoch. */
    void *owner;
} McFlexChunk;
typedef struct McFlexDevice {
    const char *id;
    const char *name;
    int32_t kind;
    uint32_t sample_rate;
    uint16_t channels;
    int32_t is_default;
} McFlexDevice;
typedef struct McFlexProcess {
    uint32_t pid;
    const char *name;
    const char *executable;
    int32_t output_active;
} McFlexProcess;
int32_t mc_flex_validate(const MediaCap_AudioSourceConfig *, McFlexError *);
int32_t mc_flex_open(const MediaCap_AudioSourceConfig *, void **stream, McFlexError *);
int32_t mc_flex_start(void *stream, McFlexError *);
int32_t mc_flex_poll(void *stream, McFlexChunk *, McFlexError *);
int32_t mc_flex_stop(void *stream, McFlexError *);
void mc_flex_free(void *stream);
void mc_flex_chunk_free(void *owner);
int32_t mc_flex_devices(void **list, size_t *count, McFlexError *);
int32_t mc_flex_device_at(void *list, size_t index, McFlexDevice *);
void mc_flex_devices_free(void *list);
int32_t mc_flex_processes(void **list, size_t *count, McFlexError *);
int32_t mc_flex_process_at(void *list, size_t index, McFlexProcess *);
void mc_flex_processes_free(void *list);
int32_t mc_flex_capabilities(MediaCap_AudioCapabilities *, McFlexError *);
void mc_flex_minimum_gated_os(uint32_t *major, uint32_t *minor, uint32_t *build);
#ifdef __cplusplus
}
#endif
