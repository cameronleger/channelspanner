#include <stdlib.h>
#include <GL/gl.h>

#include "spectrumdraw.h"
#include "units.h"
#include "logging.h"
#include "spectrumgenerator.h"

// print("{:6.3f}f, {:6.3f}f, {:6.3f}f".format(r/255.0, g/255.0, b/255.0))

#define BLACK          0.114f,  0.122f,  0.129f
#define GREY_DARK      0.157f,  0.165f,  0.180f
#define GREY           0.216f,  0.231f,  0.255f
#define GREY_LIGHT     0.294f,  0.318f,  0.337f
#define WHITE          0.773f,  0.784f,  0.776f

#define RED_DARK       0.647f,  0.259f,  0.259f
#define RED_LIGHT      0.800f,  0.400f,  0.400f
#define ORANGE_DARK    0.871f,  0.576f,  0.373f
#define ORANGE_LIGHT   0.941f,  0.776f,  0.455f
#define GREEN_DARK     0.549f,  0.580f,  0.251f
#define GREEN_LIGHT    0.710f,  0.741f,  0.408f
#define BLUE_DARK      0.373f,  0.506f,  0.616f
#define BLUE_LIGHT     0.506f,  0.635f,  0.745f
#define PINK_DARK      0.522f,  0.404f,  0.561f
#define PINK_LIGHT     0.698f,  0.580f,  0.733f

#define DB_MAX P_6_DB
#define DB_MIN N_120_DB

draw_ctx_t* init_draw_ctx( track_t* track )
{
   draw_ctx_t* ctx = malloc( sizeof( draw_ctx_t ) );
   ctx->init = 0;
   ctx->mousex = 0;
   ctx->mousey = 0;
   ctx->width = 0;
   ctx->height = 0;

   ctx->fm = track->sampleRate / 2.0f;
   ctx->fs = track->frameSize / 2 + 1;
   ctx->dl = 0.1f;
   ctx->df = track->sampleRate / track->frameSize;
   ctx->ox = 1.0f / SPECTRUM_FREQUENCY_MIN;
   ctx->oy = 1.0f / DB_MIN;
   ctx->sx = 2.0f / (logf( ctx->fm ) - logf( SPECTRUM_FREQUENCY_MIN ));
   ctx->sy = 2.0f / (logf( DB_MIN ) - logf( DB_MAX ));

   return ctx;
}

void free_draw_ctx( draw_ctx_t* ctx )
{
   if ( NULL == ctx ) return;
   free( ctx );
}

void set_mouse( draw_ctx_t* ctx, int32_t mousex, int32_t mousey )
{
   if ( NULL == ctx ) return;

   ctx->mousex = (float)mousex / ctx->width * 2 - 1;
   ctx->mousey = (float)(ctx->height - mousey) / ctx->height * 2 - 1;
}

void draw_mouse( draw_ctx_t* ctx )
{
   if ( NULL == ctx ) return;

   glLineWidth( 1.0f );
   glBegin( GL_LINES );
   glColor3f( GREY );

   glVertex2f( ctx->mousex, -1.0f );
   glVertex2f( ctx->mousex,  1.0f );

   glVertex2f( -1.0f, ctx->mousey );
   glVertex2f(  1.0f, ctx->mousey );

   glEnd();
}

void draw_grid( draw_ctx_t* ctx )
{
   if ( NULL == ctx ) return;

   float x, y;

   glLineWidth( 1.0f );
   glBegin( GL_LINES );

   for ( int i = 10; i < ctx->fm; i *= 10 )
   {
      // frequency main lines
      glColor3f( GREY_LIGHT );
      x = ctx->sx * logf( i * ctx->ox ) - 1;
      glVertex2f( x, -1.0f );
      glVertex2f( x,  1.0f );

      // frequency secondary lines
      for ( int j = i + i; j < i * 10.0f && j < ctx->fm; j += i )
      {
         glColor3f( GREY_DARK );
         x = ctx->sx * logf( j * ctx->ox ) - 1;
         glVertex2f( x, -1.0f );
         glVertex2f( x,  1.0f );
      }
   }

   // dB lines
   glColor3f( GREY );
   y = ctx->sy * logf( ctx->oy ) + 1;
   glVertex2f( -1.0f, -y );
   glVertex2f(  1.0f, -y );

   glColor3f( GREY_DARK );
   for ( int i = -108; i < 0; i += 12 )
   {
      y = ctx->sy * logf( DBTOGAIN(i) * ctx->oy ) + 1;
      glVertex2f( -1.0f, -y );
      glVertex2f(  1.0f, -y );
   }

   glEnd();
}

void draw_channel_spectrums( draw_ctx_t* ctx, track_t* track )
{
   if ( NULL == ctx ) return;
   if ( NULL == track ) return;

   float x, y, lx, a, b;

   channel_t* channel;
   for ( int ch = 0; ch < track->channelCount; ch++ ) {
      channel = &track->channels[ch];

      if ( channel->hasFFT == 0 ) continue;

      glLineWidth( 3.0f );
      glBegin( GL_LINE_STRIP );

      if ( ch == 0 ) glColor3f( BLUE_DARK );
      else glColor3f( BLUE_LIGHT );

      lx = -1.0f;

//    a = (max'-min')/(max-min) // or more simply (max'-min')
//    b = (min' - (a * min)) ) // or more simply min' + a
      a = (track->frameSize < 2048) ? 2.5f : 1.0f;
      b = (track->frameSize < 2048) ? 3.0f : 1.5f;

      for ( int i = 0; i < track->frameSize / 2 + 1; i++ )
      {
         x = ctx->sx * logf( i * ctx->df * ctx->ox ) - 1;
         if ( i == 0 ) x = -1.0f;

         y = ctx->sy * logf( channel->fft[i] * ctx->oy ) + 1;

         if ( x >  1.0f ) x =  1.0f;
         if ( x < -1.0f ) x = -1.0f;
         if ( y >  1.0f ) y =  1.1f;
         if ( y < -1.0f ) y = -1.1f;

         if ( i != 0 && (x - lx) > ctx->dl )
         {
            glVertex2f( x, -y );
            glEnd();
            glLineWidth( a * -x + b );
            glBegin( GL_LINE_STRIP );
            lx = x;
         }

         glVertex2f( x, -y );
//         if ( i % 16 == 0 )
//            DEBUG_PRINT( "%5.2f x %5.2f : %6i i %12.2f Hz %12.6f g %8.2f dB\n", x, -y, i, i * df, channel->fft[i], GAINTODB(channel->fft[i]) );
      }
      glEnd();
   }
}

void init_draw( draw_ctx_t* ctx )
{
   if ( NULL == ctx ) return;

   glEnable( GL_LINE_SMOOTH );
   glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
   glEnable( GL_BLEND );
   glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
   glDisable( GL_MULTISAMPLE );

   GLint dims[4] = {0};
   glGetIntegerv( GL_VIEWPORT, dims );
   ctx->width = dims[2];
   ctx->height = dims[3];

   ctx->init = 1;
}

void draw( draw_ctx_t* ctx, track_t* track )
{
   if ( NULL == ctx ) return;
   if ( NULL == track ) return;

   glClearColor( BLACK, 1.0 );
   glClear( GL_COLOR_BUFFER_BIT );

   draw_grid( ctx );

   draw_mouse( ctx );

   draw_channel_spectrums( ctx, track );

   glFlush();
}