#ifndef CHANNELSPANNER_DRAW_H
#define CHANNELSPANNER_DRAW_H

#include <GL/gl.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "process.h"
#include "spanner.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint tex;
   int w;
   int h;
   int x;
   int y;
   long offset;
} character_t;

typedef struct {
   size_t init;

   float mousex;
   float mousey;

   int width;
   int height;

   float swidth;
   float sheight;

   uint8_t scale;

   float sr; /* sample rate */
   float fm; /* maximum frequency to draw */

   float dl; /* delta for line-width changes, in lin units */

   float ox; /* offset/minimum for frequency graphing */
   float oy; /* offset/minimum for dB graphing */

   float sx; /* scaling factor for min/max freq range to lin range */
   float sy; /* scaling factor for min/max dB range to lin range */

   int info_dirty;
   char info_dB[10];
   char info_Hz[10];
   char info_note[8];

   float xlog[MAX_FFT];

   GLuint program;
   GLuint vao;
   GLuint vbo;

   character_t* characters;
} draw_ctx_t;

draw_ctx_t* init_draw_ctx( uint8_t scale, float sampleRate );

void free_draw_ctx( draw_ctx_t* ctx );

void set_mouse( draw_ctx_t* ctx, int32_t mousex, int32_t mousey );

void draw( draw_ctx_t* ctx, track_t* track, shared_memory_t* shmem );

#ifdef __cplusplus
}
#endif

#endif //CHANNELSPANNER_DRAW_H
