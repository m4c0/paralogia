#ifndef G3D_H
#define G3D_H

typedef void g3d_buffer_t;
typedef void g3d_pipeline_t;
typedef void g3d_sampler_t;
typedef void g3d_texture_t;

typedef struct g3d_init_s {
  void * ptr;
  g3d_buffer_t * (*new_buffer)(void * ptr, int sz);
  g3d_sampler_t * (*new_sampler)(void * ptr, int linear);
  g3d_texture_t * (*new_texture)(void * ptr, int w, int h);
  g3d_pipeline_t * (*new_pipeline)(void * ptr, const char * shader, unsigned bufs, unsigned txts);
  const void * (*load_resource)(const char * name, const char * ext, unsigned * sz);
} g3d_init_t;
int g3d_init(const g3d_init_t * t);
void g3d_deinit(void);

typedef struct g3d_frame_render_s {
  void * ptr;
  g3d_pipeline_t * pipeline;
  g3d_buffer_t ** buffers;
  g3d_texture_t ** textures;
  g3d_sampler_t ** samplers;
  unsigned instances;
} g3d_frame_render_t;
typedef struct g3d_frame_s {
  void * ptr;
  void (*load_buffer)(g3d_buffer_t * buf, const void * data, unsigned sz);
  void (*load_texture)(g3d_texture_t * txt, const void * data, unsigned w, unsigned h);
  void (*load_texture_file)(g3d_texture_t * txt, const char * name, unsigned w, unsigned h);
  void (*render)(const g3d_frame_render_t * t);
} g3d_frame_t;
int g3d_frame(const g3d_frame_t * t);

void g3d_resize(unsigned sw, unsigned sh);

void g3d_mouse_move(int x, int y);
void g3d_mouse_down(int x, int y);
void g3d_mouse_up(int x, int y);
void g3d_mouse_cancel(int x, int y);

typedef enum {
  g3d_key_up,
  g3d_key_down,
  g3d_key_left,
  g3d_key_right,
  g3d_key_action,
  g3d_key_cancel,
  g3d_key_max,
} g3d_key_t;
void g3d_key(g3d_key_t key, int down);

void g3d_scroll(float x, float y);

#endif
