@import AudioToolbox;
@import MetalKit;

#include "g3d.h"

@interface POCStuff : NSObject
@property (nonatomic,strong) NSMutableArray * objects;
@property (nonatomic,strong) id<MTLDevice> device;
@property (nonatomic,strong) id<MTLCommandQueue> queue;
+ (id)newWithDevice:(id<MTLDevice>)device;
- (void)resize:(CGSize)size;
- (void)draw:(CGSize)size rpd:(MTLRenderPassDescriptor *)rpd into:(id<CAMetalDrawable>)drawable;
@end
static g3d_buffer_t * new_buffer(void * ptr, int sz) {
  POCStuff * d = ptr;
  id<MTLBuffer> res = [d.device newBufferWithLength:sz options:MTLResourceStorageModeShared];
  [d.objects addObject:res];
  return res;
}
static g3d_pipeline_t * new_pipeline(void * ptr, const char * shader, unsigned bufs, unsigned txts) {
  POCStuff * d = ptr;

  NSString * lib_name = [NSString stringWithFormat:@"%s", shader];
  NSURL * lib_url = [[NSBundle mainBundle] URLForResource:lib_name withExtension:@"metallib"];
  if (!lib_url) return NULL;

  NSError * err;
  id<MTLLibrary> lib = [d.device newLibraryWithURL:lib_url error:&err];
  if (!lib) return NULL;

  MTLRenderPipelineDescriptor * pd = [MTLRenderPipelineDescriptor new];
  pd.vertexFunction   = [lib newFunctionWithName:@"vs_main"];
  pd.fragmentFunction = [lib newFunctionWithName:@"fs_main"];
  pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
  pd.colorAttachments[0].blendingEnabled = true;
  pd.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
  pd.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
  pd.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
  pd.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
  id<MTLRenderPipelineState> res = [d.device newRenderPipelineStateWithDescriptor:pd error:&err];
  if (err) return (NSLog(@"Error creating pipeline: %@", err), nil);

  [d.objects addObject:res];
  return res;
}
static g3d_sampler_t * new_sampler(void * ptr, int linear) {
  POCStuff * d = ptr;

  MTLSamplerDescriptor * sd = [MTLSamplerDescriptor new];
  sd.rAddressMode = sd.sAddressMode = sd.tAddressMode = MTLSamplerAddressModeClampToZero;

  if (linear) sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterLinear;
  else        sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterNearest;

  id<MTLSamplerState> res = [d.device newSamplerStateWithDescriptor:sd];
  [d.objects addObject:res];
  return res;
}
static g3d_texture_t * new_texture(void * ptr, int w, int h) {
  POCStuff * d = ptr;

  MTLTextureDescriptor * td = [MTLTextureDescriptor new];
  td.pixelFormat = MTLPixelFormatR8Unorm;
  td.width       = w;
  td.height      = h;
  id<MTLTexture> res = [d.device newTextureWithDescriptor:td];
  [d.objects addObject:res];
  return res;
}
static void load_buffer(g3d_buffer_t * buf, const void * data, unsigned sz) {
  memcpy(((id<MTLBuffer>)buf).contents, data, sz);
}
static void load_texture(g3d_texture_t * t, const void * data, unsigned w, unsigned h) {
  id<MTLTexture> txt = t;

  MTLRegion r = { {0,0,0}, {w,h,1} };
  [txt replaceRegion:r mipmapLevel:0 withBytes:data bytesPerRow:w];
}
static void load_texture_file(g3d_texture_t * t, const char * name, unsigned w, unsigned h) {
  NSString * name_s = [NSString stringWithFormat:@"%s", name];
  NSString * path = [[NSBundle mainBundle] pathForResource:name_s ofType:@"img"];
  NSData * data = [NSData dataWithContentsOfFile:path];
  load_texture(t, [data bytes], w, h);
}
static void render(const g3d_frame_render_t * t) {
  id<MTLRenderCommandEncoder> enc = t->ptr;
  [enc setRenderPipelineState:t->pipeline];
  for (int i = 0; t->buffers[i]; i++) {
    [enc setVertexBuffer:t->buffers[i] offset:0 atIndex:i];
    [enc setFragmentBuffer:t->buffers[i] offset:0 atIndex:i];
  }
  for (int i = 0; t->textures[i] && t->samplers[i]; i++) {
    [enc setFragmentTexture:t->textures[i] atIndex:i];
    [enc setFragmentSamplerState:t->samplers[i] atIndex:i];
  }
  [enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:t->instances];
}
static const void * load_resource(const char * name, const char * ext, unsigned * sz) {
  return NULL;
}
@implementation POCStuff
+ (id)newWithDevice:(id<MTLDevice>)device {
  POCStuff * d = [POCStuff new];
  d.device = device;
  d.queue = [device newCommandQueue];

  g3d_init_t api = {
    .ptr           = d,
    .new_buffer    = new_buffer,
    .new_pipeline  = new_pipeline,
    .new_sampler   = new_sampler,
    .new_texture   = new_texture,
    .load_resource = load_resource,
  };
  if (g3d_init(&api)) return nil;

  return d;
}
- (void)resize:(CGSize)size {
  g3d_resize(size.width, size.height);
}
- (void)draw:(CGSize)size rpd:(MTLRenderPassDescriptor *)rpd into:(id<CAMetalDrawable>)drawable {
  if (rpd == nil) return;

  id<MTLCommandBuffer> cb = [self.queue commandBuffer];
  id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rpd];

  g3d_frame_t api  = {
    .ptr               = enc,
    .load_buffer       = load_buffer,
    .load_texture      = load_texture,
    .load_texture_file = load_texture_file,
    .render            = render,
  };
  g3d_frame(&api);

  [enc endEncoding];

  if (drawable) [cb presentDrawable:drawable];
  [cb commit];
  if (!drawable) [cb waitUntilCompleted];
}
@end

@interface POCViewDelegate : MTKView<MTKViewDelegate>
@property (nonatomic,strong) POCStuff * stuff;
@property (nonatomic) BOOL ready;
+ (id)new;
@end
@implementation POCViewDelegate
+ (id)new {
  POCViewDelegate * d = [[POCViewDelegate alloc] init];
  d.device     = MTLCreateSystemDefaultDevice();
  d.stuff      = [POCStuff newWithDevice:d.device];
  d.clearColor = MTLClearColorMake(0.01, 0.02, 0.03, 1.0);
  d.delegate   = d;
  return d;
}
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
  if (self.ready) [self.stuff resize:view.frame.size];
}
- (void)drawInMTKView:(MTKView *)view {
  if (!self.ready) {
    g3d_resize(view.frame.size.width, view.frame.size.height);
    self.ready = YES;
  }

  MTLRenderPassDescriptor * rpd = view.currentRenderPassDescriptor;
  [self.stuff draw:view.frame.size rpd:rpd into:view.currentDrawable];
}
@end
