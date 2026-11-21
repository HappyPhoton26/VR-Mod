/* Original OpenXR/D3D11 session owner. No game hook code is borrowed. */
#include "cv_xr.h"
/* Freestanding clang emits these ABI helpers for struct initialization. */
int _fltused=0;
void *memset(void *p,int value,unsigned int n){unsigned char *b=p;while(n--)*b++=(unsigned char)value;return p;}
typedef void *HMODULE;
__declspec(dllimport) HMODULE __stdcall LoadLibraryA(const char *);
__declspec(dllimport) void * __stdcall GetProcAddress(HMODULE,const char *);
__declspec(dllimport) int __stdcall FreeLibrary(HMODULE);
typedef struct { XrStructureType type; const void *next; void *device; } Binding;
typedef struct { XrStructureType type; void *next; CvLuid luid; unsigned int level; } Requirements;
typedef struct { XrStructureType type; void *next; void *texture; } Image;
typedef XrResult (XRAPI_PTR *GetRequirements)(XrInstance,XrSystemId,Requirements *);
#define MAX_IMAGES 16
static HMODULE loader;
static XrInstance instance;
static XrSystemId system_id;
static XrSession session;
static XrSpace space, head_space;
static XrSwapchain chains[2];
static Image images[2][MAX_IMAGES];
static unsigned int counts[2],indices[2],width[2],height[2];
static int running,frame_begun,acquired[2],released[2],render_frame;
static XrTime display_time;
static unsigned long long sequence;
static XrView views[2];
static PFN_xrGetInstanceProcAddr resolve;
#define FUNCTIONS(X) \
 X(CreateInstance) X(DestroyInstance) X(GetSystem) X(GetSystemProperties) \
 X(CreateSession) X(DestroySession) X(CreateReferenceSpace) X(DestroySpace) \
 X(EnumerateViewConfigurationViews) X(EnumerateSwapchainFormats) X(CreateSwapchain) \
 X(DestroySwapchain) X(EnumerateSwapchainImages) X(PollEvent) X(BeginSession) \
 X(EndSession) X(WaitFrame) X(BeginFrame) X(LocateViews) X(LocateSpace) \
 X(AcquireSwapchainImage) X(WaitSwapchainImage) X(ReleaseSwapchainImage) X(EndFrame)
#define DECLARE(n) static PFN_xr##n p##n;
FUNCTIONS(DECLARE)
static void zero(void *p,unsigned int n){unsigned char *b=p;while(n--)*b++=0;}
static void copy_pose(CvTrackedPose *d,XrPosef p,XrSpaceLocationFlags valid){
 d->pose.orientation=(CvQuat){p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w};
 d->pose.position=(CvVec3){p.position.x,p.position.y,p.position.z};
 d->valid=0;
 if(valid & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)d->valid|=CV_POSE_ORIENTATION_VALID;
 if(valid & XR_SPACE_LOCATION_POSITION_VALID_BIT)d->valid|=CV_POSE_POSITION_VALID;
}
/* Balance a successfully begun frame even when tracking fails mid-frame. */
static XrResult abandon_frame(XrResult reason){
 XrFrameEndInfo ei={XR_TYPE_FRAME_END_INFO};ei.displayTime=display_time;
 ei.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
 pEndFrame(session,&ei);frame_begun=0;return reason;
}
#define TRY(call) do {XrResult r_=(call);if(XR_FAILED(r_))return r_;}while(0)
XrResult cvxr_open(CvLuid *luid,unsigned int *level){
 if(loader)return XR_ERROR_CALL_ORDER_INVALID;
 loader=LoadLibraryA("cod2vr_runtime.dll");
 if(!loader)loader=LoadLibraryA("openxr_loader.dll");
 if(!loader)return XR_ERROR_RUNTIME_UNAVAILABLE;
 resolve=(PFN_xrGetInstanceProcAddr)GetProcAddress(loader,"xrGetInstanceProcAddr");
 if(!resolve){cvxr_close();return XR_ERROR_FUNCTION_UNSUPPORTED;}
 XrResult result=resolve(XR_NULL_HANDLE,"xrCreateInstance",(PFN_xrVoidFunction *)&pCreateInstance);
 if(XR_FAILED(result)){cvxr_close();return result;}
 const char *extension="XR_KHR_D3D11_enable";
 XrInstanceCreateInfo ci={XR_TYPE_INSTANCE_CREATE_INFO};
 const char app[]="CoD2 VR Original";
 for(unsigned int i=0;i<sizeof(app);i++)ci.applicationInfo.applicationName[i]=app[i];
 ci.applicationInfo.apiVersion=XR_API_VERSION_1_0;
 ci.enabledExtensionCount=1;ci.enabledExtensionNames=&extension;
 result=pCreateInstance(&ci,&instance);
 if(XR_FAILED(result)){cvxr_close();return result;}
#define LOAD(n) result=resolve(instance,"xr" #n,(PFN_xrVoidFunction *)&p##n);if(XR_FAILED(result)){cvxr_close();return result;}
 FUNCTIONS(LOAD)
 XrSystemGetInfo si={XR_TYPE_SYSTEM_GET_INFO};si.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
 result=pGetSystem(instance,&si,&system_id);
 if(XR_FAILED(result)){cvxr_close();return result;}
 GetRequirements requirements=0;
 result=resolve(instance,"xrGetD3D11GraphicsRequirementsKHR",(PFN_xrVoidFunction *)&requirements);
 if(XR_FAILED(result)){cvxr_close();return result;}
 Requirements req={XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
 result=requirements(instance,system_id,&req);
 if(XR_FAILED(result)){cvxr_close();return result;}
 if(luid)*luid=req.luid;if(level)*level=req.level;
 return XR_SUCCESS;
}
XrResult cvxr_attach_d3d11(void *device){
 if(!instance || session || !device)return XR_ERROR_CALL_ORDER_INVALID;
 Binding b={XR_TYPE_GRAPHICS_BINDING_D3D11_KHR,0,device};
 XrSessionCreateInfo ci={XR_TYPE_SESSION_CREATE_INFO};ci.next=&b;ci.systemId=system_id;
 TRY(pCreateSession(instance,&ci,&session));
 XrReferenceSpaceCreateInfo rs={XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
 rs.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;rs.poseInReferenceSpace.orientation.w=1;
 TRY(pCreateReferenceSpace(session,&rs,&space));
 rs.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;
 TRY(pCreateReferenceSpace(session,&rs,&head_space));
 XrViewConfigurationView config[2]={{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}};
 unsigned int count=0;TRY(pEnumerateViewConfigurationViews(instance,system_id,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&count,config));
 if(count!=2)return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
 int64_t formats[64];TRY(pEnumerateSwapchainFormats(session,64,&count,formats));
 int64_t format=0;
 for(unsigned int i=0;i<count;i++)if(formats[i]==87)format=87;
 if(!format)return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;
 for(unsigned int eye=0;eye<2;eye++){
  width[eye]=config[eye].recommendedImageRectWidth;height[eye]=config[eye].recommendedImageRectHeight;
  XrSwapchainCreateInfo sc={XR_TYPE_SWAPCHAIN_CREATE_INFO};
  sc.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
  sc.format=format;sc.sampleCount=1;sc.width=width[eye];sc.height=height[eye];sc.faceCount=1;sc.arraySize=1;sc.mipCount=1;
  TRY(pCreateSwapchain(session,&sc,&chains[eye]));
  for(unsigned int i=0;i<MAX_IMAGES;i++)images[eye][i].type=XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
  TRY(pEnumerateSwapchainImages(chains[eye],MAX_IMAGES,&counts[eye],(XrSwapchainImageBaseHeader *)images[eye]));
 }
 return XR_SUCCESS;
}
XrResult cvxr_poll(int *is_running,int *exit_requested){
 if(!session)return XR_ERROR_CALL_ORDER_INVALID;
 if(exit_requested)*exit_requested=0;
 for(;;){
  XrEventDataBuffer event={XR_TYPE_EVENT_DATA_BUFFER};XrResult r=pPollEvent(instance,&event);
  if(r==XR_EVENT_UNAVAILABLE)break;if(XR_FAILED(r))return r;
  if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING){if(exit_requested)*exit_requested=1;running=0;}
  if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED){
   XrSessionState state=((XrEventDataSessionStateChanged *)&event)->state;
   if(state==XR_SESSION_STATE_READY){XrSessionBeginInfo begin={XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;TRY(pBeginSession(session,&begin));running=1;}
   if(state==XR_SESSION_STATE_STOPPING){running=0;TRY(pEndSession(session));}
   if(state==XR_SESSION_STATE_EXITING||state==XR_SESSION_STATE_LOSS_PENDING){running=0;if(exit_requested)*exit_requested=1;}
  }
 }
 if(is_running)*is_running=running;return XR_SUCCESS;
}
XrResult cvxr_begin(CvFrame *frame,int *should_render){
 if(!running||frame_begun||!frame)return XR_ERROR_CALL_ORDER_INVALID;
 XrFrameWaitInfo wi={XR_TYPE_FRAME_WAIT_INFO};XrFrameState fs={XR_TYPE_FRAME_STATE};
 TRY(pWaitFrame(session,&wi,&fs));XrFrameBeginInfo bi={XR_TYPE_FRAME_BEGIN_INFO};TRY(pBeginFrame(session,&bi));
 frame_begun=1;display_time=fs.predictedDisplayTime;render_frame=fs.shouldRender;
 zero(frame,sizeof(*frame));frame->version=CV_FRAME_VERSION;frame->size=sizeof(*frame);
 frame->sequence=++sequence;frame->predicted_display_time_ns=display_time;
 for(unsigned int i=0;i<2;i++){views[i].type=XR_TYPE_VIEW;acquired[i]=released[i]=0;}
 XrViewLocateInfo vi={XR_TYPE_VIEW_LOCATE_INFO};vi.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;vi.displayTime=display_time;vi.space=space;
 XrViewState vs={XR_TYPE_VIEW_STATE};unsigned int count=0;
 XrResult located=pLocateViews(session,&vi,&vs,2,&count,views);
 if(XR_FAILED(located))return abandon_frame(located);
 if(count!=2)return abandon_frame(XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED);
 if((vs.viewStateFlags&(XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT))!=(XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT))render_frame=0;
 for(unsigned int i=0;i<2;i++){
  XrSpaceLocationFlags valid=0;
  if(vs.viewStateFlags&XR_VIEW_STATE_ORIENTATION_VALID_BIT)valid|=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
  if(vs.viewStateFlags&XR_VIEW_STATE_POSITION_VALID_BIT)valid|=XR_SPACE_LOCATION_POSITION_VALID_BIT;
  copy_pose(&frame->eyes[i],views[i].pose,valid);
  frame->fov[i]=(CvFov){views[i].fov.angleLeft,views[i].fov.angleRight,views[i].fov.angleUp,views[i].fov.angleDown};
 }
 XrSpaceLocation head={XR_TYPE_SPACE_LOCATION};located=pLocateSpace(head_space,space,display_time,&head);
 if(XR_FAILED(located))return abandon_frame(located);
 copy_pose(&frame->head,head.pose,head.locationFlags);
 if(should_render)*should_render=render_frame;return XR_SUCCESS;
}
XrResult cvxr_acquire(unsigned int eye,void **texture){
 if(!frame_begun||!render_frame||eye>1||acquired[eye]||!texture)return XR_ERROR_CALL_ORDER_INVALID;
 XrSwapchainImageAcquireInfo ai={XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
 TRY(pAcquireSwapchainImage(chains[eye],&ai,&indices[eye]));acquired[eye]=1;
 XrSwapchainImageWaitInfo wi={XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=XR_INFINITE_DURATION;
 TRY(pWaitSwapchainImage(chains[eye],&wi));
 *texture=images[eye][indices[eye]].texture;return XR_SUCCESS;
}
XrResult cvxr_release(unsigned int eye){
 if(eye>1||!acquired[eye]||released[eye])return XR_ERROR_CALL_ORDER_INVALID;
 XrSwapchainImageReleaseInfo ri={XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
 TRY(pReleaseSwapchainImage(chains[eye],&ri));released[eye]=1;return XR_SUCCESS;
}
XrResult cvxr_end(int submit){
 if(!frame_begun)return XR_ERROR_CALL_ORDER_INVALID;
 XrCompositionLayerProjectionView pv[2]={{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
 for(unsigned int i=0;i<2;i++){
  if(acquired[i]&&!released[i])return XR_ERROR_CALL_ORDER_INVALID;
  pv[i].pose=views[i].pose;pv[i].fov=views[i].fov;pv[i].subImage.swapchain=chains[i];
  pv[i].subImage.imageRect.extent.width=width[i];pv[i].subImage.imageRect.extent.height=height[i];
 }
 XrCompositionLayerProjection layer={XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=space;layer.viewCount=2;layer.views=pv;
 const XrCompositionLayerBaseHeader *layers=(const XrCompositionLayerBaseHeader *)&layer;
 XrFrameEndInfo ei={XR_TYPE_FRAME_END_INFO};ei.displayTime=display_time;ei.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
 if(submit&&render_frame&&released[0]&&released[1]){ei.layerCount=1;ei.layers=&layers;}
 XrResult r=pEndFrame(session,&ei);frame_begun=0;return r;
}
void cvxr_close(void){
 if(pDestroySwapchain)for(unsigned int i=0;i<2;i++){if(chains[i])pDestroySwapchain(chains[i]);chains[i]=0;}
 if(head_space&&pDestroySpace)pDestroySpace(head_space);if(space&&pDestroySpace)pDestroySpace(space);
 if(session&&pDestroySession)pDestroySession(session);if(instance&&pDestroyInstance)pDestroyInstance(instance);
 if(loader)FreeLibrary(loader);
 loader=0;instance=0;session=0;system_id=0;space=0;head_space=0;resolve=0;running=frame_begun=0;
#define CLEAR(n) p##n=0;
 FUNCTIONS(CLEAR)
}
XrInstance cvxr_instance(void){return instance;}XrSession cvxr_session(void){return session;}
XrSpace cvxr_space(void){return space;}PFN_xrGetInstanceProcAddr cvxr_resolver(void){return resolve;}
int cvxr_eye_size(unsigned int eye,unsigned int *w,unsigned int *h){
 if(eye>1||!chains[eye]||!w||!h)return 0;*w=width[eye];*h=height[eye];return 1;
}
