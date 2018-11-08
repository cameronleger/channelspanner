#include <stdlib.h>

#include "logging.h"
#include "spectrumgenerator.h"

void window_hanning( float* samples, size_t sampleCount )
{
   for ( int i = 0; i < sampleCount; ++i )
      samples[i] = 0.5f * (1.0f - cosf( 2.0f * (float)M_PI * i / (sampleCount - 1.0f) ));
}

void init_working_area( track_t* track, size_t frameSize )
{
   track->wrk = malloc( sizeof( working_area_t ) );

   track->wrk->window = malloc( frameSize * sizeof( float ) );
   window_hanning( track->wrk->window, frameSize );

   track->wrk->kisscfg = kiss_fft_alloc( (int) frameSize, 0, 0, 0 );
   track->wrk->fftOutput = malloc( frameSize * sizeof( kiss_fft_cpx ) );
   track->wrk->samplesTmp = malloc( frameSize * sizeof( kiss_fft_cpx ) );
   track->wrk->fftTmp = malloc( (frameSize / 2 + 1) * sizeof( float ) );

   track->wrk->frameSizeInv = 1.0f / frameSize;
   track->wrk->fftSize = frameSize / 2 + 1;
}

void free_working_area( track_t* track )
{
   kiss_fft_free( track->wrk->kisscfg );
   free( track->wrk->fftOutput );
   free( track->wrk->samplesTmp );
   free( track->wrk->fftTmp );
   free( track->wrk->window );
   free( track->wrk );
}

track_t* init_sample_data( size_t frameSize )
{
   track_t* t = malloc( sizeof( track_t ) );
   t->frameSize = frameSize;
   t->color = 0;

   for ( int i = 0; i < MAX_CHANNELS; ++i )
      t->channels[i].head = 0;

   init_working_area( t, frameSize );

   DEBUG_PRINT( "Setup SampleData: %i channels x (%i samples + %i fftSamples) at %p\n", MAX_CHANNELS, MAX_FFT, (MAX_FFT / 2 + 1), t );
   return t;
}

void free_sample_data( track_t* track )
{
   if ( NULL == track ) return;

   DEBUG_PRINT( "Destroying SampleData at %p\n", track );

   free_working_area( track );
   free( track );
}

void update_frame_size( track_t* track, size_t frameSize )
{
   if ( NULL == track ) return;

   free_working_area( track );
   track->frameSize = frameSize;
   init_working_area( track, frameSize );
}

void add_sample_data( track_t* track, size_t channel, const float* samples, size_t sampleCount )
{
   if ( NULL == track ) return;

   channel_t* c = &track->channels[channel];

   if ( NULL == samples)
   {
      for ( size_t i = 0; i < sampleCount; ++i )
      {
         c->samples[c->head] = 0;
         c->head = (c->head + 1) & (track->frameSize - 1);
      }
   }
   else
   {
      for ( size_t i = 0; i < sampleCount; ++i )
      {
         c->samples[c->head] = samples[i];
         c->head = (c->head + 1) & (track->frameSize - 1);
      }
   }
}

inline void mix( const float* from, float* to, const float kFrom, const float kTo, size_t count )
{
   while ( count-- )
   {
      *to = *(to) * kTo + *(from++) * kFrom;
      to++;
   }
}

void process_samples( track_t* track, float reactivity )
{
   if ( NULL == track ) return;

   for ( size_t ch = 0; ch < MAX_CHANNELS; ++ch )
   {
      channel_t* c = &track->channels[ch];

      int hasNewValues = 0;
      float s;
      for ( size_t i = 0; i < track->frameSize; ++i )
      {
         s = c->samples[(c->head + i) & (track->frameSize - 1)];
         if ( s != 0.0f )
         {
            hasNewValues = 1;
            track->wrk->samplesTmp[i].r = track->wrk->window[i] * s;
            track->wrk->samplesTmp[i].i = 0.0f;
         }
         else
         {
            track->wrk->samplesTmp[i].r = 0.0f;
            track->wrk->samplesTmp[i].i = 0.0f;
         }
      }

      if ( hasNewValues )
      {
         kiss_fft( track->wrk->kisscfg, track->wrk->samplesTmp, track->wrk->fftOutput );

         for ( size_t i = 0; i < track->wrk->fftSize; ++i )
            track->wrk->fftTmp[i] = sqrtf(
                    track->wrk->fftOutput[i].r * track->wrk->fftOutput[i].r +
                    track->wrk->fftOutput[i].i * track->wrk->fftOutput[i].i
            ) * track->wrk->frameSizeInv;
      }
      else
         memset( track->wrk->fftTmp, 0, track->wrk->fftSize );

      int hasOldValues = 0;
      for ( size_t i = 0; i < track->wrk->fftSize; ++i )
      {
         if ( c->fft[i] > 0.0f )
         {
            hasOldValues = 1;
            break;
         }
      }

      if ( hasNewValues || hasOldValues )
         mix( track->wrk->fftTmp, c->fft, reactivity, 1.0f - reactivity, track->wrk->fftSize );

//      DEBUG_PRINT( "Processed %zu samples for channel %zu\n", track->frameSize, ch );
   }

}