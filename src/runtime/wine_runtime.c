/* Original, deliberately Wine-specific runtime dispatch adapter.
 * Negotiation ABI: Khronos OpenXR loader/runtime interface version 1.
 * Uses the installed Wine OpenXR runtime; not a general OpenXR loader.
 * Does not discover registry manifests or load API layers. */
#define XR_NO_PROTOTYPES
#include "../stereo/include/openxr/openxr.h"
typedef void *Module;
__declspec(dllimport) Module __stdcall LoadLibraryA(const char *);
__declspec(dllimport) void * __stdcall GetProcAddress(Module,const char *);
__declspec(dllimport) int __stdcall FreeLibrary(Module);
typedef struct {
    int type;
    unsigned int version, size, min_interface, max_interface;
    XrVersion min_api,max_api;
} LoaderInfo;
typedef struct {
    int type;
    unsigned int version,size,interface_version;
    XrVersion api;
    PFN_xrGetInstanceProcAddr get_proc;
} RuntimeReply;
typedef XrResult (XRAPI_PTR *Negotiate)(const LoaderInfo *,RuntimeReply *);
static Module runtime;
static PFN_xrGetInstanceProcAddr dispatch;

__declspec(dllexport) XrResult XRAPI_CALL xrGetInstanceProcAddr(
    XrInstance instance,const char *name,PFN_xrVoidFunction *function) {
    if(!name||!function)return XR_ERROR_VALIDATION_FAILURE;
    *function=0;
    /* Caller owns initialization on a single rendering thread. */
    if(!dispatch){
        Module module=LoadLibraryA("wineopenxr.dll");
        if(!module)return XR_ERROR_RUNTIME_UNAVAILABLE;
        Negotiate negotiate=(Negotiate)GetProcAddress(module,"xrNegotiateLoaderRuntimeInterface");
        if(!negotiate){FreeLibrary(module);return XR_ERROR_INITIALIZATION_FAILED;}
        LoaderInfo loader={1,1,sizeof(LoaderInfo),1,1,XR_MAKE_VERSION(1,0,0),XR_MAKE_VERSION(1,1,0)};
        RuntimeReply reply={3,1,sizeof(RuntimeReply),0,0,0};
        XrResult result=negotiate(&loader,&reply);
        if(XR_FAILED(result)||reply.interface_version!=1||!reply.get_proc){
            FreeLibrary(module);
            return XR_FAILED(result)?result:XR_ERROR_INITIALIZATION_FAILED;
        }
        runtime=module;
        dispatch=reply.get_proc;
    }
    return dispatch(instance,name,function);
}

/* Explicit release requires all sessions/instances already destroyed. */
__declspec(dllexport) void cod2vr_release_runtime(void) {
    dispatch=0;
    if(runtime)FreeLibrary(runtime);
    runtime=0;
}
