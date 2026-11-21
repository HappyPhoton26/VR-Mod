#ifndef CV_GAME_HANDS_H
#define CV_GAME_HANDS_H
#include "arms.h"
#define CV_RIG_MAX_BONES 256
typedef struct {
    unsigned count;
    int parent[CV_RIG_MAX_BONES];
    int shoulder[2],elbow[2],wrist[2];
    int wrist_twist[2];
} CvGameRig;
typedef struct {
    CvVec3 previous_elbow[2];
    unsigned previous_valid[2];
    unsigned wrist_limit_mask; /* bit0 left/bit1 right; free-hand swing<=55deg */
    unsigned anatomical_mask; /* apply 30cm upper / 26cm lower caps, at40u/m */
    unsigned contact_lock_mask; /* held-hand hint; reach never stretches arm segments */
} CvGameHandsState;
typedef struct {
    CvPose native[CV_RIG_MAX_BONES],result[CV_RIG_MAX_BONES],override[CV_RIG_MAX_BONES];
    unsigned char override_valid[CV_RIG_MAX_BONES];
} CvGameHandsScratch;
/* Resolve actual campaign bone names, never assume a fixed runtime index.
 * Parents must be parent-before-child (-1 root); full names come from the
 * active game's skeleton, not a generic VR replacement mesh. */
int cvhands_bind(CvGameRig *,unsigned count,const char *const *names,const int *parents);
/* Hook adapter boundary: native_world is the game's fully animated skeleton
 * converted by the engine adapter to active quaternion world poses. wrist[]
 * already contains controller calibration AND tracking-to-game conversion.
 * Produces world poses for the original skinned mesh; children retain their
 * original animation-relative rotations/offsets (including finger poses).
 * Inactive hands retain native animation. Does NOT write arbitrary game memory.
 * Captured animation supplies actual arm lengths, since disk rig offsets=0. */
int cvhands_retarget(const CvGameRig *,const CvPose *native_world,
    const CvTrackedPose wrist[2],CvGameHandsState *,CvPose *world_out);
/* In-game adapter should allocate scratch statically or on the heap. CoD2
 * threads have a tiny stack; this avoids ~22KB automatic pose buffers. */
int cvhands_retarget_scratch(const CvGameRig *,const CvPose *,
    const CvTrackedPose wrist[2],CvGameHandsState *,CvPose *,CvGameHandsScratch *);
/* Match renderer tracked_ref exactly, then invert actual weapon entity root.
 * ref22 words6..8 origin,9..11forward,12..14left,15..17up. entity_axes are
 * its three model-to-world basis vectors (forward/left/up), nine floats.
 * Grip-local calibration translation is in metres. */
int cvhands_tracking_to_model(const CvFrame *,const CvPose *anchor,
    const float ref22[22],const float entity_axes[9],CvVec3 entity_origin,
    float units_per_meter,const CvPose calibration[2],CvTrackedPose targets[2]);
int cvhands_clamp_wrist(const CvGameRig *,const CvPose *,CvVec3 shoulder,unsigned hand,CvVec3 target,CvVec3 *result);
int cvhands_entity_pose(const float axes[9],CvVec3 origin,CvPose *pose);
#endif
