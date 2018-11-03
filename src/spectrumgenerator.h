#ifndef CHANNELSPANNER_SPECTRUMGENERATOR_H
#define CHANNELSPANNER_SPECTRUMGENERATOR_H

#include <stdint.h>
#include <stdio.h>

#include "kiss_fft.h"

#ifdef __cplusplus
extern "C" {
#endif

// sample rate = 44100
// frame size = 2048

// note: +1 for Nyquist Frequency in KISSFFT
// bins = frame size / 2
// 1024 = 2048 / 2

// first bin = 0Hz
// last bin = (sample rate / 2) - (sample rate / frame size)
// 22028Hz = (44100 / 2) - (44100 / 2048)

// bin width = sample rate / frame size
// 21.5Hz = 44100 / 2048

// note: only 1st half of bins from KISSFFT
// for bin != 0, bin *= 2
// ignore 2nd half of bins

typedef struct {
   size_t head;
   size_t sampleCount;
   size_t hasFFT;
   float* samples;
   float* fft;
} channel_t;

typedef struct {
   size_t channelCount;
   size_t frameSize;
   float sampleRate;
   kiss_fft_cfg kisscfg;
   float* window;
   channel_t* channels;
} track_t;

track_t* init_sample_data( size_t channelCount, float sampleRate, size_t frameSize );

void free_sample_data( track_t* track );

void add_sample_data( track_t* track, size_t channel, const float* samples, size_t sampleCount );

void process_samples( track_t* track, float reactivity );

#ifdef __cplusplus
}
#endif

#endif //CHANNELSPANNER_SPECTRUMGENERATOR_H
