#ifndef CHANNELSPANNER_SPANNER_H
#define CHANNELSPANNER_SPANNER_H

#include "spectrumgenerator.h"

#define SHMEMNAME "ChannelSpanner"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   long id;
   long lastUpdate;
   size_t frameSize;
   uint8_t color;
   float fft[MAX_CHANNELS][MAX_FFT / 2 + 1];
} spanned_track_t;

typedef struct {
   size_t users;
   spanned_track_t tracks[MAX_INSTANCES];
} spanner_t;

typedef struct shared_memory_t shared_memory_t;

shared_memory_t* open_shared_memory( long id );

void close_shared_memory( shared_memory_t* shmem );

void update_shared_memory( shared_memory_t* shmem, track_t* track );

void leave_shared_memory( shared_memory_t* shmem );

spanned_track_t* get_shared_memory_tracks( shared_memory_t* shmem );

int is_this_track( shared_memory_t* shmem, spanned_track_t* track );

#ifdef __cplusplus
}
#endif

#endif //CHANNELSPANNER_SPANNER_H
