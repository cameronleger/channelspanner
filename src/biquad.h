#ifndef CHANNELSPANNER_BIQUAD_H
#define CHANNELSPANNER_BIQUAD_H

#ifdef __cplusplus
extern "C" {
#endif

#define BP_SLOPE 3

typedef struct {
   float freq;
   float q;
   double a0;
   double a1;
   double a2;
   double b1;
   double b2;
   double z1;
   double z2;
} biquad_t;

typedef struct {
   double in;
   double out;
   biquad_t biquads[BP_SLOPE];
} cascade_t;

void update_cascade( cascade_t* c, float freq, float q );

void process_cascade( cascade_t* c, const float* in, float* out, size_t sampleCount );

#ifdef __cplusplus
}
#endif

#endif //CHANNELSPANNER_BIQUAD_H
