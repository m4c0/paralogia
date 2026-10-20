#include "test-battle.com.shd.h"

#if HLSL
StructuredBuffer<glb_t> b0 : register(t0);
StructuredBuffer<pce_t> b1 : register(t1);
#endif
#define B0 B(const device glb_t * b0)
#define B1 B(const device pce_t * b1)

struct vs_out {
  float2 frag_pos LOC(0);
  uint   frag_pce LOC(1);
  float4 pos POS;
};
struct fs_out {
  float4 colour TGT(0);
};

vertex vs_out vs_main(B0 B1 uint vid VID, uint iid IID) {
  float2 fp = float2(vid & 1, (vid >> 1) & 1);

  float2 p = float2(b1[iid].pos) + fp;

  float ax = aspect(b0[0].scr_sz.x, b0[0].scr_sz.y);
  float ay = aspect(b0[0].scr_sz.y, b0[0].scr_sz.x);
  p = (p - b0[0].trans) / (b0[0].scale * float2(ax, ay));

  vs_out res;
  res.frag_pos = fp * 2 - 1;
  res.frag_pce = b1[iid].piece;
  res.pos = float4(p * float2(1, -1), 0.0f, 1.0f);
  return res;
}

fragment fs_out fs_main(vs_out vs IN) {
  float2 p = vs.frag_pos;
  float4 c = float4(0, 0, 0, 0);
  switch (vs.frag_pce) {
    case 2: c = float4(0.5, 0.5, 0.5, 1 - step(0, length(p) - 0.3)); break;
    case 3: c = float4(0.8, 0.8, 0.8, 1 - step(0, length(p) - 0.3)); break;
  }

  fs_out res;
  res.colour = c;
  return res;
}

