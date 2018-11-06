#include <stdlib.h>
#include <GL/glew.h>

#include "spectrumdraw.h"
#include "textshader.h"
#include "units.h"
#include "logging.h"

// print("{:6.3f}f, {:6.3f}f, {:6.3f}f".format(r/255.0, g/255.0, b/255.0))

#define BLACK          0.114f,  0.122f,  0.129f
#define GREY_DARK      0.157f,  0.165f,  0.180f
#define GREY           0.216f,  0.231f,  0.255f
#define GREY_LIGHT     0.294f,  0.318f,  0.337f
#define WHITE          0.773f,  0.784f,  0.776f
#define RED_DARK       0.549f,  0.220f,  0.220f
#define RED_LIGHT      0.800f,  0.400f,  0.400f
#define ORANGE_DARK    0.635f,  0.125f,  0.184f
#define ORANGE_LIGHT   0.902f,  0.435f,  0.294f
#define YELLOW_DARK    0.541f,  0.541f,  0.271f
#define YELLOW_LIGHT   0.678f,  0.675f,  0.400f
#define GREEN_DARK     0.227f,  0.490f,  0.227f
#define GREEN_LIGHT    0.251f,  0.690f,  0.251f
#define BLUE_DARK      0.294f,  0.400f,  0.490f
#define BLUE_LIGHT     0.506f,  0.635f,  0.745f
#define INDIGO_DARK    0.314f,  0.353f,  0.545f
#define INDIGO_LIGHT   0.357f,  0.412f,  0.678f
#define VIOLET_DARK    0.627f,  0.224f,  0.506f
#define VIOLET_LIGHT   0.824f,  0.224f,  0.647f

#define DB_MAX P_6_DB
#define DB_MIN N_120_DB

const char NOTES[12][3] = {"C\0", "C#\0", "D\0", "D#\0", "E\0", "F\0", "F#\0", "G\0", "G#\0", "A\0", "A#\0", "B\0"};

const float COLORS[14][3] = {
        {RED_DARK},
        {RED_LIGHT},
        {ORANGE_DARK},
        {ORANGE_LIGHT},
        {YELLOW_DARK},
        {YELLOW_LIGHT},
        {GREEN_DARK},
        {GREEN_LIGHT},
        {BLUE_DARK},
        {BLUE_LIGHT},
        {INDIGO_DARK},
        {INDIGO_LIGHT},
        {VIOLET_DARK},
        {VIOLET_LIGHT}
};

draw_ctx_t* init_draw_ctx( track_t* track, uint8_t scale, float sampleRate )
{
   draw_ctx_t* ctx = malloc( sizeof( draw_ctx_t ) );
   ctx->init = 0;
   ctx->mousex = -2.0f;
   ctx->mousey = -2.0f;
   ctx->width = 0;
   ctx->height = 0;
   ctx->scale = scale;

   ctx->fm = sampleRate / 2.0f;
   ctx->fs = track->frameSize / 2 + 1;
   ctx->dl = 0.1f;
   ctx->df = sampleRate / track->frameSize;
   ctx->ox = 1.0f / SPECTRUM_FREQUENCY_MIN;
   ctx->oy = 1.0f / DB_MIN;
   ctx->sx = 2.0f / (logf( ctx->fm ) - logf( SPECTRUM_FREQUENCY_MIN ));
   ctx->sy = 2.0f / (logf( DB_MIN ) - logf( DB_MAX ));

   ctx->characters = malloc( 128 * sizeof( character_t ) );
   ctx->program = 0;

   return ctx;
}

void free_draw_ctx( draw_ctx_t* ctx )
{
   if ( NULL == ctx ) return;
   free( ctx->characters );
   free( ctx );
}

void set_mouse( draw_ctx_t* ctx, int32_t mousex, int32_t mousey )
{
   if ( NULL == ctx ) return;

   ctx->mousex = (float) mousex / ctx->width * 2 - 1;
   ctx->mousey = (float) (ctx->height - mousey) / ctx->height * 2 - 1;
}

void draw_text( draw_ctx_t* ctx, const char* c, size_t charCount, float _x, float _y, float sx, float sy, float r, float g, float b, int halign, int valign )
{
   glUseProgram( ctx->program );
   glUniform3f( glGetUniformLocation( ctx->program, "textColor" ), r, g, b );
   glActiveTexture( GL_TEXTURE0 );
   glBindVertexArray( ctx->vao );

   if ( halign || valign )
   {
      float lx = 0.0f;
      float mh = 0.0f;
      for ( int i = 0; i < charCount; i++ )
      {
         if ( c[i] == 0 ) break;

         lx += (ctx->characters[c[i]].offset / 64) * sx;

         float _mh = ctx->characters[c[i]].h * sy;
         mh = (_mh > mh) ? _mh : mh;
      }
      if ( halign ) _x -= lx;
      if ( valign ) _y -= mh;
   }

   for ( int i = 0; i < charCount; i++ )
   {
      if ( c[i] == 0 ) break;
      character_t ch = ctx->characters[c[i]];

      GLfloat x = _x + ch.x * sx;
      GLfloat y = _y - (ch.h - ch.y) * sy;
      GLfloat w = ch.w * sx;
      GLfloat h = ch.h * sy;

      GLfloat vertices[6][4] = {
              { x,     y + h,   0.0, 0.0 },
              { x,     y,       0.0, 1.0 },
              { x + w, y,       1.0, 1.0 },

              { x,     y + h,   0.0, 0.0 },
              { x + w, y,       1.0, 1.0 },
              { x + w, y + h,   1.0, 0.0 }
      };

      glBindTexture( GL_TEXTURE_2D, ch.tex );
      glBindBuffer( GL_ARRAY_BUFFER, ctx->vbo );
      glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices );
      glBindBuffer( GL_ARRAY_BUFFER, 0 );
      glDrawArrays( GL_TRIANGLES, 0, 6 );
      _x += (ch.offset / 64) * sx;
   }

   glBindVertexArray( 0 );
   glBindTexture( GL_TEXTURE_2D, 0 );
   glUseProgram( 0 );
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

void draw_info( draw_ctx_t* ctx )
{
   float sx = 2.0f / ctx->width;
   float sy = 2.0f / ctx->height;

   float gain = expf( (-ctx->mousey - 1) / ctx->sy ) / ctx->oy;

   char info_dB[10];
   snprintf( info_dB, 10, "%6.1f dB", GAINTODB( gain ) );
   draw_text( ctx, info_dB, 10, ctx->mousex + 0.01f, ctx->mousey + 0.02f, sx, sy, WHITE, 0, 0 );

   float freq = expf( (ctx->mousex + 1) / ctx->sx ) / ctx->ox;

   char info_Hz[10];
   snprintf( info_Hz, 10, "%6.0f Hz", freq );
   draw_text( ctx, info_Hz, 10, ctx->mousex - 0.01f, ctx->mousey + 0.02f, sx, sy, WHITE, 1, 0 );

   if ( freq > C0 )
   {
      float octave = log2f( freq / C0 );
      float semitone = 12 * octave;
      float cent = 100 * semitone;

      int note = (int) semitone % 12;
      note = (note < 0) ? -note : note;

      int offset = (int) cent % 100;
      if ( offset > 50 )
      {
         offset = offset - 100;
         note += 1;
         if ( note == 12 )
         {
            note = 0;
            octave += 1;
         }
      }

      char info_note[8];
      snprintf( info_note, 8, "%-2s%i%+-3i", NOTES[note], (int) floorf( octave ), offset );
      draw_text( ctx, info_note, 8, ctx->mousex - 0.01f, ctx->mousey - 0.02f, sx, sy, WHITE, 1, 1 );
   }
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
      y = ctx->sy * logf( DBTOGAIN( i ) * ctx->oy ) + 1;
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
   for ( int ch = 0; ch < MAX_CHANNELS; ch++ )
   {
      channel = &track->channels[ch];

      glLineWidth( 3.0f );
      glBegin( GL_LINE_STRIP );

      int colorOffset = (ch % 2 == 0) ? 0 : 1;
      glColor3f( COLORS[track->color * 2 + colorOffset][0],
                 COLORS[track->color * 2 + colorOffset][1],
                 COLORS[track->color * 2 + colorOffset][2] );

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
   glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

   GLint dims[4] = {0};
   glGetIntegerv( GL_VIEWPORT, dims );
   ctx->width = dims[2];
   ctx->height = dims[3];

   int e;

   glewExperimental = GL_TRUE;
   e = glewInit();
   if ( e != GLEW_OK )
      DEBUG_PRINT( "GLEW Initialization failed: %i\n", e );
   // GLEW generates GL error because it calls glGetString(GL_EXTENSIONS), we'll consume it here.
   glGetError();

   FT_Library freetype;
   FT_Face fontface;

   e = FT_Init_FreeType( &freetype );
   if ( e )
      DEBUG_PRINT( "Unable to load FreeType: %i\n", e );

   e = FT_New_Face( freetype, "/usr/share/fonts/TTF/DejaVuSansMono.ttf", 0, &fontface );
   if ( e )
      DEBUG_PRINT( "Unable to load FreeType Font: %i\n", e );
   FT_Set_Pixel_Sizes( fontface, 0, 8u * ctx->scale );

   for ( GLubyte c = 0; c < 128; c++ )
   {
      if ( FT_Load_Char( fontface, c, FT_LOAD_RENDER ) )
      {
         DEBUG_PRINT( "Unable to load character: %i, %c\n", c, c );
         continue;
      }
      GLuint tex;
      glGenTextures( 1, &tex );
      glBindTexture( GL_TEXTURE_2D, tex );
      glTexImage2D(
              GL_TEXTURE_2D,
              0/*level*/,
              GL_RED,
              fontface->glyph->bitmap.width,
              fontface->glyph->bitmap.rows,
              0/*border*/,
              GL_RED,
              GL_UNSIGNED_BYTE,
              fontface->glyph->bitmap.buffer
      );
      glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
      glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
      glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
      glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

      character_t ch = {
              tex,
              fontface->glyph->bitmap.width,
              fontface->glyph->bitmap.rows,
              fontface->glyph->bitmap_left,
              fontface->glyph->bitmap_top,
              fontface->glyph->advance.x
      };
      ctx->characters[c] = ch;
   }

   FT_Done_Face( fontface );
   FT_Done_FreeType( freetype );

   glGenVertexArrays( 1, &ctx->vao );
   glGenBuffers( 1, &ctx->vbo );
   glBindVertexArray( ctx->vao );
   glBindBuffer( GL_ARRAY_BUFFER, ctx->vbo );
   glBufferData( GL_ARRAY_BUFFER, sizeof( GLfloat ) * 6 * 4, NULL, GL_DYNAMIC_DRAW );
   glEnableVertexAttribArray( 0 );
   glVertexAttribPointer( 0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof( GLfloat ), 0 );
   glBindBuffer( GL_ARRAY_BUFFER, 0 );
   glBindVertexArray( 0 );

   ctx->program = create_shader();

   ctx->init = 1;
}

void draw( draw_ctx_t* ctx, track_t* track )
{
   if ( NULL == ctx ) return;
   if ( NULL == track ) return;

   if ( ctx-> init == 0 ) init_draw( ctx );

   glClearColor( BLACK, 1.0 );
   glClear( GL_COLOR_BUFFER_BIT );

   draw_grid( ctx );

   draw_mouse( ctx );

   draw_channel_spectrums( ctx, track );

   draw_info ( ctx );

   glFlush();
}