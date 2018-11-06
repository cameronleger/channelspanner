#include <stdlib.h>

#include "logging.h"
#include "spectrumgenerator.h"

void window_hanning( float* samples, size_t sampleCount )
{
   for ( int i = 0; i < sampleCount; i++ )
   {
      samples[i] = 0.5f * (1.0f - cosf( 2.0f * (float)M_PI * i / (sampleCount - 1.0f) ));
   }
}

track_t* init_sample_data( size_t frameSize )
{
   track_t* data = malloc( sizeof( track_t ) );
   data->frameSize = frameSize;
   data->color = 0;

   data->window = malloc( frameSize * sizeof( float ) );
   window_hanning( data->window, frameSize );

   data->kisscfg = kiss_fft_alloc( (int) frameSize, 0, 0, 0 );

   for ( int i = 0; i < MAX_CHANNELS; i++ )
   {
      data->channels[i].head = 0;
   }

   DEBUG_PRINT( "Setup SampleData: %i channels x (%i samples + %i fftSamples) at %p\n", MAX_CHANNELS, MAX_FFT, (MAX_FFT / 2 + 1), data );
   return data;
}

void free_sample_data( track_t* track )
{
   if ( NULL == track ) return;

   DEBUG_PRINT( "Destroying SampleData at %p\n", track );

   kiss_fft_free( track->kisscfg );
   track->kisscfg = NULL;

   free( track->window );
   free( track );
}

void add_sample_data( track_t* track, size_t channel, const float* samples, size_t sampleCount )
{
   if ( NULL == track ) return;

   channel_t* c = &track->channels[channel];

   for ( size_t i = 0; i < sampleCount; i++ )
   {
      c->samples[c->head] = samples[i];
      c->head = (c->head + 1) % track->frameSize;
   }
}

void mix( const float* from, float* to, const float kFrom, const float kTo, size_t count )
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

   size_t fftSize = track->frameSize / 2 + 1;
   kiss_fft_cpx samples[track->frameSize];
   kiss_fft_cpx fftResult[track->frameSize];
   float fft[fftSize];

   for ( size_t ch = 0; ch < MAX_CHANNELS; ch++ )
   {
      channel_t* c = &track->channels[ch];

      for ( int i = 0; i < track->frameSize; i++ )
      {
         samples[i].r = track->window[i] * c->samples[(c->head + i) % track->frameSize];
         samples[i].i = 0.0f;
      }

      kiss_fft( track->kisscfg, samples, fftResult );

      // TMP
//      float df = track->sampleRate / track->frameSize;

      for ( int i = 0; i < fftSize; i++ )
      {
         fft[i] = sqrtf( (fftResult[i].r * fftResult[i].r + fftResult[i].i * fftResult[i].i) ) / track->frameSize;

//         if ( i % 16 == 0 )
//            DEBUG_PRINT( "%5i i %12.2f Hz %12.6f g %8.2f dB\n", i, i * df, c->fft[i], GAINTODB( c->fftResult[i] ) );
      }

      mix( fft, c->fft, reactivity, 1.0f - reactivity, fftSize );

//      DEBUG_PRINT( "Processed %zu samples for channel %zu\n", track->frameSize, ch );
   }

}