#ifndef CV_VR_POSE_H
#define CV_VR_POSE_H
/* Original project ABI. OpenXR right-handed metres: +Y up, -Z forward.
 * Fixed-width fields; do not send this struct across processes without
 * explicitly negotiating packing, version and size. */
#define CV_FRAME_VERSION 1u
#define CV_HAND_LEFT 0u
#define CV_HAND_RIGHT 1u
#define CV_POSE_ORIENTATION_VALID 1u
#define CV_POSE_POSITION_VALID 2u
typedef struct { float x,y,z; } CvVec3;
typedef struct { float x,y,z,w; } CvQuat;
typedef struct { CvQuat orientation; CvVec3 position; } CvPose;
typedef struct { CvPose pose; unsigned int valid; } CvTrackedPose;
typedef struct { float angle_left,angle_right,angle_up,angle_down; } CvFov;
typedef struct {
    unsigned int version,size;
    unsigned long long sequence;
    long long predicted_display_time_ns;
    CvTrackedPose head,eyes[2],grip[2],aim[2];
    CvFov fov[2];
} CvFrame;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(float) == 4, "CvFrame requires 32-bit float");
_Static_assert(sizeof(unsigned int) == 4, "CvFrame requires 32-bit unsigned int");
_Static_assert(sizeof(long long) == 8, "CvFrame requires 64-bit long long");
_Static_assert(sizeof(CvTrackedPose) == 32, "Unexpected pose packing");
_Static_assert(sizeof(CvFrame) == 280, "Unexpected frame packing");
#endif
#endif
