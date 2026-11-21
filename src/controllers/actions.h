#ifndef CV_CONTROLLER_ACTIONS_H
#define CV_CONTROLLER_ACTIONS_H
#define XR_NO_PROTOTYPES
#include "../stereo/include/openxr/openxr.h"
#include "../include/vr_pose.h"
#include "../include/vr_input.h"
/* Call init after session/reference-space creation, before xrBeginSession.
 * Only one owner may attach action sets to the session: include any other
 * gameplay sets in this subsystem before attaching them. Render-thread only. */
XrResult cvcontrols_init(XrInstance,XrSession,XrSpace,PFN_xrGetInstanceProcAddr);
/* Must use the SAME predicted timestamp as eye/head tracking. Invalid/inactive
 * inputs are cleared every update; stale controller poses are never retained. */
XrResult cvcontrols_update(CvFrame *);
/* Snapshot gameplay actions sampled by the most recent update/sync. */
void cvcontrols_buttons(CvButtonsFrame *);
void cvcontrols_close(void);
#endif
