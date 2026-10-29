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
  int index;
} brd_t;
typedef struct mov_s {
  int2 from;
  int2 to;
} mov_t;
typedef struct pce_s {
  int2 pos;
  int2 target;
  int  piece;
  int  order;
} pce_t;

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

static inline float4 c_movement(float4 c, float2 p, int2 from, int2 to) {
  if (all(to == from)) return c;
  if (any(from < 0) || any(to < 0)) return c;

  float2 pa = p - float2(from) - 0.5;
  float2 ba = float2(to - from);
  float h = clamp(dot(pa, ba) / dot(ba, ba), 0.f, 1.f);
  float d = step(0, length(pa - ba * h) - 0.05);

  d = min(d, step(0, length(p - float2(to) - 0.5) - 0.15));

  return mix(c, float4(0.5, 0, 0, 1.0), 1 - d);
}
#endif
