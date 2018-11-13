#ifndef CHANNELSPANNER_PROCESS_H
#define CHANNELSPANNER_PROCESS_H

#include <stdint.h>
#include <fftw3.h>

#ifdef __cplusplus
extern "C" {
#endif

// sample rate = 44100
// frame size = 2048

// note: +1 for Nyquist Frequency in "real-only" input
// bins = frame size / 2
// 1024 = 2048 / 2

// first bin = 0Hz
// last bin = (sample rate / 2) - (sample rate / frame size)
// 22028Hz = (44100 / 2) - (44100 / 2048)

// bin width = sample rate / frame size
// 21.5Hz = 44100 / 2048

// note: only 1st half of bins from "real-only" input
// for bin != 0, bin *= 2
// ignore 2nd half of bins

typedef struct {
   size_t fftSize;
   float frameSizeInv;
   fftwf_plan fftw;
   float* window;
   float* samplesTmp;
   fftwf_complex* fftOutput;
   float* fftTmp;
} working_area_t;

typedef struct {
   size_t head;
   float samples[MAX_FFT];
   float fft[MAX_FFT / 2 + 1];
} channel_t;

typedef struct {
   size_t frameSize;
   uint8_t color;
   uint8_t group;
   channel_t channels[MAX_CHANNELS];
   working_area_t* wrk;
} track_t;

track_t* init_sample_data( size_t frameSize );

void free_sample_data( track_t* track );

void update_frame_size( track_t* track, size_t frameSize );

void add_sample_data( track_t* track, size_t channel, const float* samples, size_t sampleCount );

void process_samples( track_t* track, float reactivity );

#ifdef __cplusplus
}
#endif

#endif //CHANNELSPANNER_PROCESS_H
