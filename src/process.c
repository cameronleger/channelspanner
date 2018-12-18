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

   track->wrk->window = fftwf_alloc_real( frameSize );
   window_hanning( track->wrk->window, frameSize );

   track->wrk->samplesTmp = fftwf_alloc_real( frameSize );
   track->wrk->fftOutput = fftwf_alloc_complex( track->wrk->fftSize );
   track->wrk->fftTmp = fftwf_alloc_real( track->wrk->fftSize );

   track->wrk->fftw = fftwf_plan_dft_r2c_1d( (int)frameSize, track->wrk->samplesTmp, track->wrk->fftOutput, FFTW_PATIENT );

   DEBUG_PRINT( "Setup Working area: %i samples + %i fftSamples at %p\n", MAX_FFT, (MAX_FFT / 2 + 1), track->wrk );
}

void free_working_area( track_t* track )
{
   fftwf_destroy_plan( track->wrk->fftw );
   fftwf_free( track->wrk->fftOutput );
   fftwf_free( track->wrk->samplesTmp );
   fftwf_free( track->wrk->fftTmp );
   fftwf_free( track->wrk->window );
   fftwf_free( track->wrk );
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

void add_sample_data( track_t* track, size_t channel, const float* samples, const size_t sampleCount )
{
   if ( NULL == track ) return;

   channel_t* c = &track->channels[channel];

   size_t head = c->head;
   size_t frameSize = track->frameSize;

   if ( head + sampleCount <= frameSize )
   {
      if ( NULL == samples)
         for ( size_t i = 0; i < sampleCount; ++i )
            c->samples[head + i] = 0;
      else
         for ( size_t i = 0; i < sampleCount; ++i )
            c->samples[head + i] = samples[i];
   }
   else
   {
      if ( NULL == samples)
      {
         for ( size_t i = head; i < frameSize; ++i )
            c->samples[i] = 0;
         size_t j = sampleCount - (frameSize - head);
         for ( size_t i = 0; i < j; ++i )
            c->samples[i] = 0;
      }
      else
      {
         size_t n = 0;
         for ( size_t i = head; i < frameSize; ++i, ++n )
            c->samples[i] = samples[n];
         size_t j = sampleCount - (frameSize - head);
         for ( size_t i = 0; i < j; ++i, ++n )
            c->samples[i] = samples[n];
      }
   }
   c->head = (head + sampleCount) % frameSize;
}

void process_samples( track_t* track, float reactivity )
{
   if ( NULL == track ) return;

   size_t frameSize = track->frameSize;
   size_t fftSize = track->wrk->fftSize;

   for ( size_t ch = 0; ch < MAX_CHANNELS; ++ch )
   {
      channel_t* c = &track->channels[ch];

      size_t j = 0, h = c->head;
      for ( size_t i = h; i < frameSize; ++i, ++j )
         track->wrk->samplesTmp[j] = track->wrk->window[j] * c->samples[i];
      for ( size_t i = 0; i < h; ++i, ++j )
         track->wrk->samplesTmp[j] = track->wrk->window[j] * c->samples[i];

      int hasNewValues = 0;
      for ( size_t i = 0; i < frameSize; ++i )
         if ( track->wrk->samplesTmp[i] != 0.0f ) {
            hasNewValues = 1;
            break;
         }

      if ( hasNewValues )
      {
         fftwf_execute( track->wrk->fftw );

         for ( size_t i = 0; i < fftSize; ++i )
            track->wrk->fftTmp[i] = sqrtf(
                    track->wrk->fftOutput[i][0] * track->wrk->fftOutput[i][0] +
                    track->wrk->fftOutput[i][1] * track->wrk->fftOutput[i][1]
            ) * track->wrk->frameSizeInv;
      }
      else
         for ( size_t i = 0; i < fftSize; ++i )
            track->wrk->fftTmp[i] = 0;

      int hasOldValues = 0;
      for ( size_t i = 0; i < fftSize; ++i )
         if ( c->fft[i] > 0.0f )
         {
            hasOldValues = 1;
            break;
         }

      if ( hasNewValues || hasOldValues )
      {
         if ( reactivity == 1.0f )
            for ( size_t i = 0; i < fftSize; ++i )
               c->fft[i] = track->wrk->fftTmp[i];
         else
         {
            const float kFrom = reactivity;
            const float kTo = 1.0f - kFrom;
            for ( size_t i = 0; i < fftSize; ++i )
               c->fft[i] = c->fft[i] * kTo + track->wrk->fftTmp[i] * kFrom;
         }
      }

//      DEBUG_PRINT( "Processed %zu samples for channel %zu\n", track->frameSize, ch );
   }
}
