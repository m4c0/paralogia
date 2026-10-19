#include "common.shd.h"

typedef struct glb_s {
  int2   brd_sz;
  float2 scr_sz;
  int2   mouse;
  int2   pick;
  float2 trans;
  float  scale;
  float  p0;
} glb_t;
typedef struct brd_s {
  int piece;
} brd_t;
typedef struct mov_s {
  int2 from;
  int2 to;
} mov_t;

static inline float aspect(float a, float b) {
  return (a > b) ? a / b : 1;
}
static inline float frag_pos_x(float x, glb_t glb) {
  return glb.scale * aspect(glb.scr_sz.x, glb.scr_sz.y) * (x * 2.f - 1.f) + glb.trans.x;
}
static inline float frag_pos_y(float y, glb_t glb) {
  return glb.scale * aspect(glb.scr_sz.y, glb.scr_sz.x) * (y * 2.f - 1.f) + glb.trans.y;
}

#if HLSL || METAL
static inline float2 frag_pos(float2 p, glb_t glb) {
  return float2(frag_pos_x(p.x, glb), frag_pos_y(p.y, glb));
}
#endif
