#include "common.shd.h"

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
fragment fs_out fs_main() {
  fs_out res;
  res.colour = float4(1, 1, 1, 1);
  return res;
}
