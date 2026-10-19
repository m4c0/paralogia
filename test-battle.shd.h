#include "test-battle.com.shd.h"

#if HLSL
StructuredBuffer<glb_t> b0 : register(t0);
StructuredBuffer<brd_t> b1 : register(t1);
#endif
#define B0 B(const device glb_t * b0)
#define B1 B(const device brd_t * b1)

struct vs_out {
  float2 frag_pos LOC(0);
  float4 pos      POS;
};
struct fs_out {
  float4 colour TGT(0);
};

vertex vs_out vs_main(uint vid VID) {
  float2 fp = float2(vid & 1, (vid >> 1) & 1);
  float2 p = fp * 2 - 1;

  vs_out res;
  res.frag_pos = fp;
  res.pos = float4(p * float2(1, -1), 0.0f, 1.0f);
  return res;
}

static inline float3 c_border(float3 c, float2 p) {
  float2 dd = step(0.98, abs(p));
  return c * (1 - 0.8 * max(dd.x, dd.y));
}

static inline float3 c_hover(float3 c, float2 p) {
  return c * 0.1;
}
static inline float3 c_pick(float3 c, float2 p) {
  return float3(c.x, c.yz * 0.1);
}

static inline float3 c_inside(B1 float3 c, float2 p, glb_t vsin) {
  if (any(p < 0)) return c;
  if (any(p >= float2(vsin.brd_sz))) return c;

  int2 id = int2(floor(p));
  float2 uv = fract(p) * 2 - 1;

  c = mix(
      float3(0.1, 0.15, 0.2),
      float3(0.15, 0.2, 0.25),
      (id.x + id.y) & 1);

  c = c_border(c, uv);
  if (all(id == vsin.mouse)) c = c_hover(c, uv);
  if (all(id == vsin.pick)) c = c_pick(c, uv);

  uint i = id.y * uint(vsin.brd_sz.x) + id.x;
  switch (b1[i].piece) {
    case 1: c = 0.3; break;
    case 2: c = mix(0.5, c, step(0, length(uv) - 0.3)); break;
    case 3: c = mix(0.8, c, step(0, length(uv) - 0.3)); break;
  }

  return c;
}

fragment fs_out fs_main(B0 B1 vs_out vs IN) {
  float2 p = frag_pos(vs.frag_pos, b0[0]);

  float3 c = float3(0.05, 0.10, 0.15);
  c = c_inside(B(b1) c, p, b0[0]);

  fs_out res;
  res.colour = float4(c, 1);
  return res;
}
