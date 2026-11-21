#ifndef CV_XR_H
#define CV_XR_H
#define XR_NO_PROTOTYPES
#include "include/openxr/openxr.h"
#include "../include/vr_pose.h"
#ifdef _WIN32
#define CVXR_API __declspec(dllexport)
#else
#define CVXR_API
#endif
/* D3D11 device and texture pointers retain their usual COM ownership rules.
 * Caller creates the device on adapter_luid and copies each eye image into
 * the acquired texture before release. All calls belong to the render thread. */
typedef struct { unsigned int low; int high; } CvLuid;
CVXR_API XrResult cvxr_open(CvLuid *adapter_luid, unsigned int *feature_level);
CVXR_API XrResult cvxr_attach_d3d11(void *device);
CVXR_API XrResult cvxr_poll(int *running, int *exit_requested);
CVXR_API XrResult cvxr_begin(CvFrame *frame, int *should_render);
CVXR_API XrResult cvxr_acquire(unsigned int eye, void **texture);
CVXR_API XrResult cvxr_release(unsigned int eye);
CVXR_API XrResult cvxr_end(int submit_stereo);
CVXR_API void cvxr_close(void);
/* Controllers may resolve OpenXR action functions through this shared owner. */
CVXR_API XrInstance cvxr_instance(void);
CVXR_API XrSession cvxr_session(void);
CVXR_API XrSpace cvxr_space(void);
CVXR_API PFN_xrGetInstanceProcAddr cvxr_resolver(void);
CVXR_API int cvxr_eye_size(unsigned int eye,unsigned int *width,unsigned int *height);
#endif
