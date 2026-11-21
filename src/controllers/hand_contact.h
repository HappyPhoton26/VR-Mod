#ifndef CV_HAND_CONTACT_H
#define CV_HAND_CONTACT_H
#include "../include/vr_pose.h"
/* Exported read-only-by-consumer diagnostic data in the guest DLL. Snapshot
 * only when matching even publish_sequence surrounds the copy. Positions are
 * in the active weapon entity's model coordinates; errors in game units. */
typedef struct {
    unsigned magic,version,size,publish_sequence;
    unsigned long long frame_sequence;
    unsigned held,active_mask;
    float left_squeeze,error[2],shoulder_shift[2];
    CvPose target[2],solved[2];
    CvVec3 native_shoulder[2],solved_shoulder[2];
} CvHandContactDebug;
#endif
