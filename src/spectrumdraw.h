#ifndef CHANNELSPANNER_SPECTRUMDRAW_H
#define CHANNELSPANNER_SPECTRUMDRAW_H

#include "spectrumgenerator.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   size_t init;

   float mousex;
   float mousey;

   int width;
   int height;

   float fm; /* maximum frequncy to draw */
   size_t fs; /* count of frequency bins for drawing */

   float dl; /* delta for line-width changes, in lin units */
   float df; /* delta for frequency spacing, in lin units, based on bin widths */

   float ox; /* offset/minimum for frequency graphing */
   float oy; /* offset/minimum for dB graphing */

   float sx; /* scaling factor for min/max freq range to lin range */
   float sy; /* scaling factor for min/max dB range to lin range */

} draw_ctx_t;

draw_ctx_t* init_draw_ctx( track_t* track );

void free_draw_ctx( draw_ctx_t* ctx );

void set_mouse( draw_ctx_t* ctx, int32_t mousex, int32_t mousey );

void init_draw( draw_ctx_t* ctx );

void draw( draw_ctx_t* ctx, track_t* track );

#ifdef __cplusplus
}
#endif

#endif //CHANNELSPANNER_SPECTRUMDRAW_H
