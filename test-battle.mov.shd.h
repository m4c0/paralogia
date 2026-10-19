#include "test-battle.com.shd.h"

#if HLSL
StructuredBuffer<glb_t> b0 : register(t0);
#endif
#define B0 B(const device glb_t * b0)

struct vs_out {
  float2 frag_pos LOC(0);
  float4 pos      POS;
};
struct fs_out {
  float4 colour TGT(0);
};

vertex vs_out vs_main(B0 uint vid VID) {
  float2 fp = float2(vid & 1, (vid >> 1) & 1);

  const float2 p0 = float2(b0[0].pick);
  const float2 p1 = float2(b0[0].mouse) + float2(1, 1);
  float2 pp = p0 + fp * (p1 - p0);
  float ax = aspect(b0[0].scr_sz.x, b0[0].scr_sz.y);
  float ay = aspect(b0[0].scr_sz.y, b0[0].scr_sz.x);
  pp = (pp - b0[0].trans) / (b0[0].scale * float2(ax, ay));

  vs_out res;
  res.frag_pos = fp;
  res.pos = float4(pp * float2(1, -1), 0.0f, 1.0f);
  return res;
}
fragment fs_out fs_main(vs_out vs IN) {
  fs_out res;
  res.colour = float4(vs.frag_pos, 0, 1);
  return res;
}
