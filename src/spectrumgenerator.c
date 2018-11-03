#include <stdlib.h>

#include "logging.h"
#include "spectrumgenerator.h"
#include "units.h"

void window_hanning( float* samples, size_t sampleCount )
{
   for ( int i = 0; i < sampleCount; i++ )
   {
      samples[i] = 0.5f * (1.0f - cosf( 2.0f * M_PI * i / (sampleCount - 1.0f) ));
   }
}

track_t* init_sample_data( size_t channelCount, float sampleRate, size_t frameSize )
{
   track_t* data = malloc( sizeof( track_t ) + sizeof( channel_t[channelCount] ) );
   data->channelCount = channelCount;
   data->frameSize = frameSize;
   data->sampleRate = sampleRate;

   data->window = malloc( frameSize * sizeof( float ) );
   window_hanning( data->window, frameSize );

   data->kisscfg = kiss_fft_alloc( (int) frameSize, 0, 0, 0 );
   data->channels = calloc( (size_t) channelCount, sizeof( channel_t ) );

   for ( size_t i = 0; i < channelCount; i++ )
   {
      data->channels[i].samples = malloc( frameSize * sizeof( float ) );
      data->channels[i].fft = malloc( (frameSize / 2 + 1) * sizeof( float ) );
      data->channels[i].head = 0;
      data->channels[i].sampleCount = 0;
      data->channels[i].hasFFT = 0;
   }

   DEBUG_PRINT( "Setup SampleData: %zu channels x (%zu samples + %zu fftSamples)\n", data->channelCount, data->frameSize, (frameSize / 2 + 1) );
   return data;
}

void free_sample_data( track_t* track )
{
   if ( NULL == track ) return;

   for ( size_t i = 0; i < track->channelCount; i++ )
   {
      free( track->channels[i].samples );
      track->channels[i].samples = NULL;
      free( track->channels[i].fft );
      track->channels[i].fft = NULL;
   }

   kiss_fft_free( track->kisscfg );
   track->kisscfg = NULL;

   free( track->channels );
   free( track );

   DEBUG_PRINT( "Destroyed SampleData\n" );
}

void add_sample_data( track_t* track, size_t channel, const float* samples, size_t sampleCount )
{
   if ( NULL == track ) return;

   channel_t* c = &track->channels[channel];

   for ( size_t i = 0; i < sampleCount; i++ )
   {
      c->samples[c->head] = samples[i];
      c->head = (c->head + 1) % track->frameSize;
      if ( c->sampleCount < track->frameSize )
         c->sampleCount += 1;
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

   for ( size_t ch = 0; ch < track->channelCount; ch++ )
   {
      channel_t* c = &track->channels[ch];
      if ( c->sampleCount < track->frameSize )
      {
         c->hasFFT = 0;
         DEBUG_PRINT( "Ignoring channel %zu with %zu < %zu samples\n", ch, c->sampleCount, track->frameSize );
         continue;
      }

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
         fft[i] = sqrtf( (fftResult[i].r * fftResult[i].r + fftResult[i].i * fftResult[i].i) ) / ( track->frameSize / 4 );

//         if ( i % 16 == 0 )
//            DEBUG_PRINT( "%5i i %12.2f Hz %12.6f g %8.2f dB\n", i, i * df, c->fft[i], GAINTODB( c->fftResult[i] ) );
      }

      mix( fft, c->fft, reactivity, 1.0f - reactivity, fftSize );

      c->hasFFT = 1;

//      DEBUG_PRINT( "Processed %zu samples for channel %zu\n", track->frameSize, ch );
   }

}