/* Offline session-contract test. This is not a headset/rendering test. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define __declspec(x)
#define __stdcall
/* Avoid colliding with the hosted CRT's memset prototype. */
#define memset cv_test_memset
#include "cv_xr.c"
#undef memset
static int event_sent,end_layers,destroyed;
static XrResult XRAPI_CALL noop(void){return XR_SUCCESS;}
static XrResult XRAPI_CALL create(const XrInstanceCreateInfo *ci,XrInstance *out){assert(ci->enabledExtensionCount==1);*out=(XrInstance)1;return XR_SUCCESS;}
static XrResult XRAPI_CALL get_system(XrInstance i,const XrSystemGetInfo *si,XrSystemId *out){(void)i;(void)si;*out=1;return XR_SUCCESS;}
static XrResult XRAPI_CALL requirements(XrInstance i,XrSystemId s,Requirements *r){(void)i;(void)s;r->luid.low=42;r->level=0xb000;return XR_SUCCESS;}
static XrResult XRAPI_CALL create_session(XrInstance i,const XrSessionCreateInfo *ci,XrSession *out){(void)i;assert(((const Binding *)ci->next)->device==(void *)123);*out=(XrSession)2;return XR_SUCCESS;}
static XrResult XRAPI_CALL create_space(XrSession s,const XrReferenceSpaceCreateInfo *ci,XrSpace *out){(void)s;*out=(XrSpace)(uintptr_t)(ci->referenceSpaceType+3);return XR_SUCCESS;}
static XrResult XRAPI_CALL configs(XrInstance i,XrSystemId s,XrViewConfigurationType t,uint32_t cap,uint32_t *count,XrViewConfigurationView *v){(void)i;(void)s;(void)t;assert(cap==2);*count=2;for(int n=0;n<2;n++){v[n].recommendedImageRectWidth=100;v[n].recommendedImageRectHeight=120;}return XR_SUCCESS;}
static XrResult XRAPI_CALL formats(XrSession s,uint32_t cap,uint32_t *count,int64_t *f){(void)s;(void)cap;*count=1;*f=87;return XR_SUCCESS;}
static XrResult XRAPI_CALL chain(XrSession s,const XrSwapchainCreateInfo *ci,XrSwapchain *out){(void)s;assert(ci->width==100&&ci->height==120);*out=(XrSwapchain)4;return XR_SUCCESS;}
static XrResult XRAPI_CALL chain_images(XrSwapchain sc,uint32_t cap,uint32_t *count,XrSwapchainImageBaseHeader *out){(void)sc;(void)cap;*count=1;((Image *)out)->texture=(void *)456;return XR_SUCCESS;}
static XrResult XRAPI_CALL poll(XrInstance i,XrEventDataBuffer *out){(void)i;if(event_sent++)return XR_EVENT_UNAVAILABLE;out->type=XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;((XrEventDataSessionStateChanged *)out)->state=XR_SESSION_STATE_READY;return XR_SUCCESS;}
static XrResult XRAPI_CALL wait_frame(XrSession s,const XrFrameWaitInfo *wi,XrFrameState *out){(void)s;(void)wi;out->predictedDisplayTime=123456;out->shouldRender=1;return XR_SUCCESS;}
static XrResult XRAPI_CALL locate(XrSession s,const XrViewLocateInfo *li,XrViewState *vs,uint32_t cap,uint32_t *count,XrView *v){(void)s;assert(li->displayTime==123456&&cap==2);*count=2;vs->viewStateFlags=XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT;for(int n=0;n<2;n++){v[n].pose.orientation.w=1;v[n].pose.position.x=n?0.032f:-0.032f;v[n].fov=(XrFovf){-0.7f,0.7f,0.8f,-0.8f};}return XR_SUCCESS;}
static XrResult XRAPI_CALL head_location(XrSpace s,XrSpace b,XrTime t,XrSpaceLocation *out){(void)s;(void)b;assert(t==123456);out->locationFlags=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;out->pose.orientation.w=1;out->pose.position.y=1.7f;return XR_SUCCESS;}
static XrResult XRAPI_CALL acquire(XrSwapchain s,const XrSwapchainImageAcquireInfo *i,uint32_t *index){(void)s;(void)i;*index=0;return XR_SUCCESS;}
static XrResult XRAPI_CALL end_frame(XrSession s,const XrFrameEndInfo *ei){(void)s;end_layers=ei->layerCount;if(ei->layerCount){const XrCompositionLayerProjection *p=(const XrCompositionLayerProjection *)ei->layers[0];assert(p->viewCount==2);assert(p->views[0].pose.position.x<0&&p->views[1].pose.position.x>0);}return XR_SUCCESS;}
static XrResult XRAPI_CALL destroy_instance(XrInstance i){(void)i;destroyed++;return XR_SUCCESS;}
static XrResult XRAPI_CALL resolver(XrInstance i,const char *name,PFN_xrVoidFunction *out){(void)i;
#define MAP(n,f) if(!strcmp(name,"xr" #n)){*out=(PFN_xrVoidFunction)f;return XR_SUCCESS;}
 MAP(CreateInstance,create) MAP(GetSystem,get_system) MAP(GetD3D11GraphicsRequirementsKHR,requirements)
 MAP(CreateSession,create_session) MAP(CreateReferenceSpace,create_space)
 MAP(EnumerateViewConfigurationViews,configs) MAP(EnumerateSwapchainFormats,formats)
 MAP(CreateSwapchain,chain) MAP(EnumerateSwapchainImages,chain_images) MAP(PollEvent,poll)
 MAP(WaitFrame,wait_frame) MAP(LocateViews,locate) MAP(LocateSpace,head_location)
 MAP(AcquireSwapchainImage,acquire) MAP(EndFrame,end_frame) MAP(DestroyInstance,destroy_instance)
 *out=(PFN_xrVoidFunction)noop;return XR_SUCCESS;
}
HMODULE LoadLibraryA(const char *name){assert(!strcmp(name,"cod2vr_runtime.dll"));return (void *)1;}
void *GetProcAddress(HMODULE m,const char *name){(void)m;assert(!strcmp(name,"xrGetInstanceProcAddr"));return (void *)resolver;}
int FreeLibrary(HMODULE m){(void)m;return 1;}
int main(void){
 CvLuid luid;unsigned int level,w,h;CvFrame frame;int active,quit,render;void *texture;
 assert(cvxr_begin(&frame,&render)==XR_ERROR_CALL_ORDER_INVALID);
 assert(cvxr_open(&luid,&level)==XR_SUCCESS&&luid.low==42&&level==0xb000);
 assert(cvxr_attach_d3d11((void *)123)==XR_SUCCESS);
 assert(cvxr_eye_size(0,&w,&h)&&w==100&&h==120);
 assert(cvxr_poll(&active,&quit)==XR_SUCCESS&&active&&!quit);
 assert(cvxr_begin(&frame,&render)==XR_SUCCESS&&render);
 assert(frame.version==CV_FRAME_VERSION&&frame.predicted_display_time_ns==123456);
 assert(frame.head.valid==3&&frame.head.pose.position.y==1.7f);
 assert(frame.eyes[0].pose.position.x==-0.032f&&frame.eyes[1].pose.position.x==0.032f);
 assert(cvxr_begin(&frame,&render)==XR_ERROR_CALL_ORDER_INVALID);
 for(unsigned int e=0;e<2;e++){
  assert(cvxr_acquire(e,&texture)==XR_SUCCESS&&texture==(void *)456);
  assert(cvxr_end(1)==XR_ERROR_CALL_ORDER_INVALID);
  assert(cvxr_release(e)==XR_SUCCESS);
 }
 assert(cvxr_end(1)==XR_SUCCESS&&end_layers==1);
 assert(cvxr_end(1)==XR_ERROR_CALL_ORDER_INVALID);
 cvxr_close();assert(destroyed==1);cvxr_close();assert(destroyed==1);
 puts("OpenXR session mock: passed (no headset rendering validated)");return 0;
}
