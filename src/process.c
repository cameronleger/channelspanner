#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "logging.h"
#include "process.h"

void window_hanning( float* samples, size_t sampleCount )
{
   for ( int i = 0; i < sampleCount; ++i )
      samples[i] = 0.5f * (1.0f - cosf( 2.0f * (float)M_PI * i / (sampleCount - 1.0f) ));
}

void init_working_area( track_t* track, size_t frameSize )
{
   track->wrk = malloc( sizeof( working_area_t ) );
   memset( track->wrk, 0, sizeof( working_area_t ) );

   track->wrk->fftSize = frameSize / 2 + 1;
   track->wrk->frameSizeInv = 1.0f / frameSize;

   track->wrk->samplesTmp = fftwf_alloc_real( frameSize );
   track->wrk->window = malloc( frameSize * sizeof( float ) );
   window_hanning( track->wrk->window, frameSize );

   track->wrk->fftOutput = fftwf_alloc_complex( track->wrk->fftSize );
   track->wrk->fftTmp = malloc( track->wrk->fftSize * sizeof( float ) );
   memset( track->wrk->fftTmp, 0, track->wrk->fftSize * sizeof( float ) );

   track->wrk->fftw = fftwf_plan_dft_r2c_1d( (int)frameSize, track->wrk->samplesTmp, track->wrk->fftOutput, FFTW_PATIENT );

   DEBUG_PRINT( "Setup Working area: %i samples + %i fftSamples at %p\n", MAX_FFT, (MAX_FFT / 2 + 1), track->wrk );
}

void free_working_area( track_t* track )
{
   fftwf_destroy_plan( track->wrk->fftw );
   fftwf_free( track->wrk->fftOutput );
   fftwf_free( track->wrk->samplesTmp );
   free( track->wrk->fftTmp );
   free( track->wrk->window );
   free( track->wrk );
}

track_t* init_sample_data( size_t frameSize )
{
   track_t* t = malloc( sizeof( track_t ) );
   memset( t, 0, sizeof( track_t ) );
   t->frameSize = frameSize;
   t->color = 0;
   t->group = 1;

   for ( int i = 0; i < MAX_CHANNELS; ++i )
   {
      t->channels[i].head = 0;
      memset( &t->channels[i].fft[0], 0, (MAX_FFT / 2 + 1) * sizeof( float ) );
   }

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

void process_samples( track_t* track, float reactivity )
{
   if ( NULL == track ) return;

   for ( size_t ch = 0; ch < MAX_CHANNELS; ++ch )
   {
      channel_t* c = &track->channels[ch];

      int hasNewValues = 0;
      float s = 0;
      for ( size_t i = 0; i < track->frameSize; ++i )
      {
         s = c->samples[(c->head + i) & (track->frameSize - 1)];
         if ( s != 0.0f ) hasNewValues = 1;
         track->wrk->samplesTmp[i] = track->wrk->window[i] * s;
      }

      if ( hasNewValues )
      {
         fftwf_execute( track->wrk->fftw );
//         DEBUG_PRINT( "FFTW Cost: %10.10f\n", fftwf_cost( track->wrk->fftw ) );

         for ( size_t i = 0; i < track->wrk->fftSize; ++i )
            track->wrk->fftTmp[i] = sqrtf(
                    track->wrk->fftOutput[i][0] * track->wrk->fftOutput[i][0] +
                    track->wrk->fftOutput[i][1] * track->wrk->fftOutput[i][1]
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
      {
         const float kFrom = reactivity;
         const float kTo = 1.0f - reactivity;
         for ( size_t i = 0; i < track->wrk->fftSize; ++i )
            c->fft[i] = c->fft[i] * kTo + track->wrk->fftTmp[i] * kFrom;
      }

//      DEBUG_PRINT( "Processed %zu samples for channel %zu\n", track->frameSize, ch );
   }

}