#include "../include/vr_scope.h"
static CvScopeTransport *scope_data;
static unsigned char scope_pixels[CV_SCOPE_SLOT_BYTES];
static unsigned scope_sequence,scope_uploaded,scope_visible;
static double scope_received_at;
static GLuint scope_texture,scope_fbo;
static XrSwapchain scope_chain;
static XrSwapchainImageOpenGLKHR scope_images[16];
static void scope_viewer_init(XrSession session,int64_t format){
 scope_data=open_map("/home/steamos/cod2-vr-work/transport/scope.bin",CV_SCOPE_MAP_BYTES);
 __atomic_store_n(&scope_data->magic,0,__ATOMIC_RELEASE);
 XrSwapchainCreateInfo ci={XR_TYPE_SWAPCHAIN_CREATE_INFO};
 ci.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
 ci.format=format;ci.sampleCount=1;ci.width=ci.height=CV_SCOPE_SIZE;ci.faceCount=ci.arraySize=ci.mipCount=1;
 CHECK(xrCreateSwapchain(session,&ci,&scope_chain));
 for(unsigned i=0;i<16;i++)scope_images[i].type=XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
 uint32_t n;CHECK(xrEnumerateSwapchainImages(scope_chain,16,&n,(XrSwapchainImageBaseHeader *)scope_images));
 glGenFramebuffers(1,&scope_fbo);glGenTextures(1,&scope_texture);glBindTexture(GL_TEXTURE_2D,scope_texture);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,CV_SCOPE_SIZE,CV_SCOPE_SIZE,0,GL_BGRA,GL_UNSIGNED_BYTE,0);
}
static void receive_scope(void){
 unsigned sequence=__atomic_load_n(&scope_data->sequence,__ATOMIC_ACQUIRE);
 if(!sequence||sequence==scope_sequence)return;
 CvScopeTransport h;memcpy(&h,scope_data,sizeof h);
 if(h.magic!=CV_SCOPE_MAGIC||h.version!=CV_SCOPE_VERSION||h.width!=CV_SCOPE_SIZE||h.height!=CV_SCOPE_SIZE||h.stride!=CV_SCOPE_SIZE*4||h.slot>2)return;
 if(!h.reserved[0]){scope_visible=0;scope_sequence=sequence;return;}
 unsigned generation=__atomic_load_n(&scope_data->generation[h.slot],__ATOMIC_ACQUIRE);
 if(!generation||(generation&1))return;
 memcpy(scope_pixels,(unsigned char *)scope_data+64+h.slot*CV_SCOPE_SLOT_BYTES,CV_SCOPE_SLOT_BYTES);
 __atomic_thread_fence(__ATOMIC_ACQUIRE);
 if(generation!=__atomic_load_n(&scope_data->generation[h.slot],__ATOMIC_ACQUIRE))return;
 for(unsigned i=3;i<CV_SCOPE_SLOT_BYTES;i+=4)scope_pixels[i]=255;
 glBindTexture(GL_TEXTURE_2D,scope_texture);
 glTexSubImage2D(GL_TEXTURE_2D,0,0,0,CV_SCOPE_SIZE,CV_SCOPE_SIZE,GL_BGRA,GL_UNSIGNED_BYTE,scope_pixels);
 scope_sequence=sequence;scope_visible=1;scope_received_at=clock_s();
}
static void scope_draw_circle(void){
 glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,scope_texture);glColor4f(1,1,1,1);
 glBegin(GL_TRIANGLE_FAN);glTexCoord2f(.5f,.5f);glVertex2f(0,0);
 for(unsigned i=0;i<=128;i++){
  float a=(float)i*6.28318530718f/128,c=cosf(a),s=sinf(a);
  glTexCoord2f(.5f+.5f*c,.5f-.5f*s);glVertex2f(.94f*c,.94f*s);
 }
 glEnd();glDisable(GL_TEXTURE_2D);glColor4f(0,0,0,1);
 glBegin(GL_TRIANGLE_STRIP);
 for(unsigned i=0;i<=128;i++){
  float a=(float)i*6.28318530718f/128,c=cosf(a),s=sinf(a);
  glVertex2f(.94f*c,.94f*s);glVertex2f(.985f*c,.985f*s);
 }
 glEnd();
 /* Shared monoscopic reticle follows the same muzzle direction as firing. */
 glLineWidth(2);glBegin(GL_LINES);
 glVertex2f(-.94f,0);glVertex2f(.94f,0);glVertex2f(0,-.94f);glVertex2f(0,.94f);
 glEnd();glLineWidth(1);
}
static int scope_quad_layer(XrSpace head_space,XrCompositionLayerQuad *layer){
 if(!scope_visible||hud_menu||clock_s()-scope_received_at>.5)return 0;
 if(scope_uploaded!=scope_sequence){
  uint32_t index;XrSwapchainImageAcquireInfo ai={XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  CHECK(xrAcquireSwapchainImage(scope_chain,&ai,&index));
  XrSwapchainImageWaitInfo wi={XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=XR_INFINITE_DURATION;
  CHECK(xrWaitSwapchainImage(scope_chain,&wi));
  glBindFramebuffer(GL_FRAMEBUFFER,scope_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,scope_images[index].image,0);
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){fprintf(stderr,"Scope framebuffer incomplete\n");exit(4);}
  glViewport(0,0,CV_SCOPE_SIZE,CV_SCOPE_SIZE);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
  scope_draw_circle();glFlush();glBindFramebuffer(GL_FRAMEBUFFER,0);
  XrSwapchainImageReleaseInfo ri={XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};CHECK(xrReleaseSwapchainImage(scope_chain,&ri));
  scope_uploaded=scope_sequence;
 }
 *layer=(XrCompositionLayerQuad){XR_TYPE_COMPOSITION_LAYER_QUAD};
 layer->layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;layer->space=head_space;
 layer->eyeVisibility=XR_EYE_VISIBILITY_BOTH;layer->pose.orientation.w=1;layer->pose.position.z=-1.5f;
 layer->size=(XrExtent2Df){1.1f,1.1f};layer->subImage.swapchain=scope_chain;
 layer->subImage.imageRect.extent=(XrExtent2Di){CV_SCOPE_SIZE,CV_SCOPE_SIZE};return 1;
}
static void scope_viewer_close(void){
 if(scope_chain)xrDestroySwapchain(scope_chain);
 glDeleteTextures(1,&scope_texture);glDeleteFramebuffers(1,&scope_fbo);
 munmap(scope_data,CV_SCOPE_MAP_BYTES);
}
