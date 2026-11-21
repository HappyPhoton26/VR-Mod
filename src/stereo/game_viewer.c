/* Original native compositor for independently rendered CoD2 eye images. */
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <GL/gl.h>
#include <GL/glext.h>
#define XR_USE_PLATFORM_EGL
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "../controllers/actions.h"
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static volatile sig_atomic_t stop;
static void stopped(int s){(void)s;stop=1;}
#define CHECK(call) do{XrResult rr=(call);if(XR_FAILED(rr)){fprintf(stderr,"%s: OpenXR error %d\n",#call,rr);exit(1);}}while(0)
static double clock_s(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
#include "../transport/viewer_io.h"
#include "../transport/viewer_hud.h"
#include "../transport/viewer_scope.h"
int main(void){
 setvbuf(stdout,0,_IOLBF,0);signal(SIGINT,stopped);signal(SIGTERM,stopped);
 fprintf(stdout,"Original CoD2 eye-image compositor; waiting for game frames.\n");
 EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);if(!eglInitialize(d,0,0)){fprintf(stderr,"eglInitialize failed: 0x%x\n",eglGetError());return 1;}
 eglBindAPI(EGL_OPENGL_API);
 EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
 EGLConfig cfg;EGLint count;if(!eglChooseConfig(d,attrs,&cfg,1,&count)||!count)return 2;
 EGLint ca[]={EGL_CONTEXT_MAJOR_VERSION,4,EGL_CONTEXT_MINOR_VERSION,3,EGL_CONTEXT_OPENGL_PROFILE_MASK,EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,EGL_NONE};
 EGLContext ctx=eglCreateContext(d,cfg,EGL_NO_CONTEXT,ca);
 if(ctx==EGL_NO_CONTEXT||!eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,ctx)){fprintf(stderr,"EGL context failed 0x%x\n",eglGetError());return 2;}
 printf("GL renderer: %s\n",glGetString(GL_RENDERER));
 const char *extensions[]={"XR_MNDX_egl_enable","XR_KHR_opengl_enable","XR_VALVE_frame_controller_interaction"};
 uint32_t ext_count=0,enabled=2;CHECK(xrEnumerateInstanceExtensionProperties(0,0,&ext_count,0));
 XrExtensionProperties *ep=calloc(ext_count,sizeof(*ep));if(!ep)return 2;
 for(uint32_t i=0;i<ext_count;i++)ep[i].type=XR_TYPE_EXTENSION_PROPERTIES;
 CHECK(xrEnumerateInstanceExtensionProperties(0,ext_count,&ext_count,ep));
 for(uint32_t i=0;i<ext_count;i++)if(!strcmp(ep[i].extensionName,extensions[2]))enabled=3;
 free(ep);
 XrInstanceCreateInfo ici={XR_TYPE_INSTANCE_CREATE_INFO};strcpy(ici.applicationInfo.applicationName,"Original CoD2 VR");ici.applicationInfo.apiVersion=XR_API_VERSION_1_0;ici.enabledExtensionCount=enabled;ici.enabledExtensionNames=extensions;
 XrInstance instance;fprintf(stdout,"Creating OpenXR instance...\n");CHECK(xrCreateInstance(&ici,&instance));
 XrSystemGetInfo gi={XR_TYPE_SYSTEM_GET_INFO};gi.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;XrSystemId sys;CHECK(xrGetSystem(instance,&gi,&sys));
 PFN_xrGetOpenGLGraphicsRequirementsKHR requirements;CHECK(xrGetInstanceProcAddr(instance,"xrGetOpenGLGraphicsRequirementsKHR",(PFN_xrVoidFunction *)&requirements));
 XrGraphicsRequirementsOpenGLKHR req={XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};CHECK(requirements(instance,sys,&req));
 XrGraphicsBindingEGLMNDX binding={XR_TYPE_GRAPHICS_BINDING_EGL_MNDX};binding.getProcAddress=eglGetProcAddress;binding.display=d;binding.config=cfg;binding.context=ctx;
 XrSessionCreateInfo sci={XR_TYPE_SESSION_CREATE_INFO};sci.next=&binding;sci.systemId=sys;XrSession session;CHECK(xrCreateSession(instance,&sci,&session));
 XrReferenceSpaceCreateInfo ri={XR_TYPE_REFERENCE_SPACE_CREATE_INFO};ri.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;ri.poseInReferenceSpace.orientation.w=1;XrSpace space;CHECK(xrCreateReferenceSpace(session,&ri,&space));
 ri.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;XrSpace head_space;CHECK(xrCreateReferenceSpace(session,&ri,&head_space));
 CHECK(cvcontrols_init(instance,session,space,xrGetInstanceProcAddr));
 transport_init();hud_viewer_init();
 XrViewConfigurationView vc[2]={{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}};uint32_t n;CHECK(xrEnumerateViewConfigurationViews(instance,sys,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&n,vc));if(n!=2)return 3;
 int64_t formats[64];CHECK(xrEnumerateSwapchainFormats(session,64,&n,formats));int64_t fmt=0;for(unsigned int i=0;i<n;i++)if(formats[i]==GL_SRGB8_ALPHA8)fmt=GL_SRGB8_ALPHA8;if(!fmt){fprintf(stderr,"No sRGB swapchain format\n");return 3;}
 scope_viewer_init(session,fmt);
 XrSwapchain sc[2];XrSwapchainImageOpenGLKHR images[2][16];GLuint fbo[2],depth[2];
 for(int e=0;e<2;e++){
  XrSwapchainCreateInfo ci={XR_TYPE_SWAPCHAIN_CREATE_INFO};ci.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;ci.format=fmt;ci.sampleCount=1;ci.width=vc[e].recommendedImageRectWidth;ci.height=vc[e].recommendedImageRectHeight;ci.faceCount=ci.arraySize=ci.mipCount=1;CHECK(xrCreateSwapchain(session,&ci,&sc[e]));
  memset(images[e],0,sizeof(images[e]));for(int j=0;j<16;j++)images[e][j].type=XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;CHECK(xrEnumerateSwapchainImages(sc[e],16,&n,(XrSwapchainImageBaseHeader *)images[e]));
  glGenFramebuffers(1,&fbo[e]);glGenRenderbuffers(1,&depth[e]);glBindRenderbuffer(GL_RENDERBUFFER,depth[e]);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,ci.width,ci.height);
  printf("Eye %d: %u x %u, images %u\n",e,ci.width,ci.height,n);
 }
 int active=0;unsigned long frames=0;double last=0;
 while(!stop){
  XrEventDataBuffer event={XR_TYPE_EVENT_DATA_BUFFER};
  while(xrPollEvent(instance,&event)==XR_SUCCESS){
   if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED){XrSessionState state=((XrEventDataSessionStateChanged *)&event)->state;printf("Session state %d\n",state);
    if(state==XR_SESSION_STATE_READY){XrSessionBeginInfo begin={XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;CHECK(xrBeginSession(session,&begin));active=1;}
    if(state==XR_SESSION_STATE_STOPPING){active=0;CHECK(xrEndSession(session));}
    if(state==XR_SESSION_STATE_EXITING||state==XR_SESSION_STATE_LOSS_PENDING)stop=1;
   }if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)stop=1;event.type=XR_TYPE_EVENT_DATA_BUFFER;
  }
  if(!active){usleep(10000);continue;}
  XrFrameWaitInfo wi={XR_TYPE_FRAME_WAIT_INFO};XrFrameState fs={XR_TYPE_FRAME_STATE};CHECK(xrWaitFrame(session,&wi,&fs));XrFrameBeginInfo bi={XR_TYPE_FRAME_BEGIN_INFO};CHECK(xrBeginFrame(session,&bi));
  XrViewLocateInfo li={XR_TYPE_VIEW_LOCATE_INFO};li.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;li.displayTime=fs.predictedDisplayTime;li.space=space;
  XrViewState vs={XR_TYPE_VIEW_STATE};XrView views[2]={{XR_TYPE_VIEW},{XR_TYPE_VIEW}};CHECK(xrLocateViews(session,&li,&vs,2,&n,views));
  CvFrame tracking={0};tracking.version=CV_FRAME_VERSION;tracking.size=sizeof(tracking);tracking.predicted_display_time_ns=fs.predictedDisplayTime;
  XrResult controls=cvcontrols_update(&tracking);if(XR_FAILED(controls)&&controls!=XR_SESSION_NOT_FOCUSED){fprintf(stderr,"Controller update: %d\n",controls);}
  CvButtonsFrame buttons;cvcontrols_buttons(&buttons);publish_game_input(&buttons);
  tracking.sequence=frames+1;
  for(unsigned e=0;e<2;e++){
   XrPosef pose=views[e].pose;
   tracking.eyes[e].pose.orientation=(CvQuat){pose.orientation.x,pose.orientation.y,pose.orientation.z,pose.orientation.w};
   tracking.eyes[e].pose.position=(CvVec3){pose.position.x,pose.position.y,pose.position.z};
   tracking.eyes[e].valid=(unsigned int)vs.viewStateFlags&3u;
   tracking.fov[e]=(CvFov){views[e].fov.angleLeft,views[e].fov.angleRight,views[e].fov.angleUp,views[e].fov.angleDown};
  }
  XrSpaceLocation head={XR_TYPE_SPACE_LOCATION};CHECK(xrLocateSpace(head_space,space,fs.predictedDisplayTime,&head));
  tracking.head.pose.orientation=(CvQuat){head.pose.orientation.x,head.pose.orientation.y,head.pose.orientation.z,head.pose.orientation.w};
  tracking.head.pose.position=(CvVec3){head.pose.position.x,head.pose.position.y,head.pose.position.z};
  tracking.head.valid=(unsigned int)head.locationFlags&3u;
  publish_tracking(&tracking);receive_images();receive_hud();receive_scope();
  XrCompositionLayerProjectionView pv[2]={{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
  int draw=fs.shouldRender&&n==2&&(vs.viewStateFlags&XR_VIEW_STATE_ORIENTATION_VALID_BIT)&&(vs.viewStateFlags&XR_VIEW_STATE_POSITION_VALID_BIT);
  int draw_menu=draw&&hud_menu&&hud_fresh();
  int draw_world=draw&&game_world_fresh()&&!draw_menu;
  if(draw_world||draw_menu)for(int e=0;e<2;e++){
   uint32_t index;XrSwapchainImageAcquireInfo ai={XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};CHECK(xrAcquireSwapchainImage(sc[e],&ai,&index));XrSwapchainImageWaitInfo sw={XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};sw.timeout=XR_INFINITE_DURATION;CHECK(xrWaitSwapchainImage(sc[e],&sw));
   glBindFramebuffer(GL_FRAMEBUFFER,fbo[e]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,images[e][index].image,0);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth[e]);
   if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){fprintf(stderr,"Incomplete eye framebuffer\n");return 4;}
   glViewport(0,0,vc[e].recommendedImageRectWidth,vc[e].recommendedImageRectHeight);glEnable(GL_FRAMEBUFFER_SRGB);glEnable(GL_DEPTH_TEST);glClearColor(.015f,.02f,.04f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
   XrFovf image_fov=draw_world?rendered_eye_fov((unsigned)e):views[e].fov;
   if(draw_world)draw_game_eye((unsigned)e);
   if(draw_menu||!hud_menu)draw_hud_eye(image_fov);
   glFlush();glBindFramebuffer(GL_FRAMEBUFFER,0);XrSwapchainImageReleaseInfo sr={XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};CHECK(xrReleaseSwapchainImage(sc[e],&sr));
   pv[e].pose=draw_world?rendered_eye_pose((unsigned)e):views[e].pose;pv[e].fov=image_fov;pv[e].subImage.swapchain=sc[e];pv[e].subImage.imageRect.extent.width=vc[e].recommendedImageRectWidth;pv[e].subImage.imageRect.extent.height=vc[e].recommendedImageRectHeight;
  }
  XrCompositionLayerProjection layer={XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=space;layer.viewCount=2;layer.views=pv;
  XrCompositionLayerQuad scope_layer;
  const XrCompositionLayerBaseHeader *layers[2]={(const XrCompositionLayerBaseHeader *)&layer,0};
  unsigned layer_count=draw_world||draw_menu?1u:0u;
  if(draw_world&&scope_quad_layer(head_space,&scope_layer))layers[layer_count++]=(const XrCompositionLayerBaseHeader *)&scope_layer;
  XrFrameEndInfo ei={XR_TYPE_FRAME_END_INFO};ei.displayTime=fs.predictedDisplayTime;ei.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;ei.layerCount=layer_count;ei.layers=layer_count?layers:0;CHECK(xrEndFrame(session,&ei));
  if(draw)frames++;
  if(clock_s()-last>5){float dx=views[1].pose.position.x-views[0].pose.position.x,dy=views[1].pose.position.y-views[0].pose.position.y,dz=views[1].pose.position.z-views[0].pose.position.z;printf("Submitted compositor frames %lu; game uploads %lu; eye separation %.4f m; pose validity 0x%llx; grips %u/%u\n",frames,game_uploads,sqrtf(dx*dx+dy*dy+dz*dz),(unsigned long long)vs.viewStateFlags,tracking.grip[0].valid,tracking.grip[1].valid);last=clock_s();}
 }
 printf("Exiting; submitted %lu stereo frames\n",frames);
 scope_viewer_close();hud_viewer_close();transport_close();xrDestroySpace(head_space);cvcontrols_close();for(int e=0;e<2;e++){xrDestroySwapchain(sc[e]);glDeleteFramebuffers(1,&fbo[e]);glDeleteRenderbuffers(1,&depth[e]);}xrDestroySpace(space);xrDestroySession(session);xrDestroyInstance(instance);eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,ctx);eglTerminate(d);return 0;
}
