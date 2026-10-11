#ifndef NETPULSE_COLLECTOR_H
#define NETPULSE_COLLECTOR_H
#include <windows.h>
#include <stdint.h>
#define TRACK_PATH 520
#define TRACK_MAGIC 0x4e505433u
typedef struct { uint32_t magic,error; int64_t stamp; uint64_t down,up,lost; wchar_t path[TRACK_PATH]; } TrackPacket;
int collector_run(const wchar_t *pipe_name,DWORD parent_id);
int tracker_start(HWND owner);
void tracker_stop(void);
void tracker_request_stop(void);
/* 1 packet, 0 no packet, -1 disconnected */
int tracker_read(TrackPacket *packet);
int tracker_enabled(void);
#endif
