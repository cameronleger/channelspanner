
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "biquad.h"
#include "logging.h"

inline void update_biquad( biquad_t* bq )
{
   double k = tan( M_PI * bq->freq );
   double k2 = k * k;
   double kkQ = k / bq->q;
   double norm = 1 / (1 + kkQ + k2);
   bq->a0 = kkQ * norm;
   bq->a1 = 0;
   bq->a2 = -bq->a0;
   bq->b1 = 2 * (k2 - 1) * norm;
   bq->b2 = (1 - kkQ + k2) * norm;
}

inline void update_cascade( cascade_t* c, float freq, float q )
{
   c->biquads[0].freq = freq;
   c->biquads[0].q = q;
   update_biquad( &c->biquads[0] );
   for ( size_t i = 1; i < BP_SLOPE; ++i )
   {
      c->biquads[i].freq = c->biquads[0].freq;
      c->biquads[i].a0 = c->biquads[0].a0;
      c->biquads[i].a1 = c->biquads[0].a1;
      c->biquads[i].a2 = c->biquads[0].a2;
      c->biquads[i].b1 = c->biquads[0].b1;
      c->biquads[i].b2 = c->biquads[0].b2;
   }
}

inline void process_cascade( cascade_t* c, const float* in, float* out, size_t sampleCount )
{
   for ( size_t s = 0; s < sampleCount; ++s )
   {
      c->in = in[s];
      c->out = in[s];
      for ( size_t bq = 0; bq < BP_SLOPE; ++bq )
      {
         c->out = c->in * c->biquads[bq].a0 + c->biquads[bq].z1;
         c->biquads[bq].z1 = c->in * c->biquads[bq].a1 + c->biquads[bq].z2 - c->biquads[bq].b1 * c->out;
         c->biquads[bq].z2 = c->in * c->biquads[bq].a2 - c->biquads[bq].b2 * c->out;
         c->in = c->out;
      }
      out[s] = c->out;
   }
}