#ifndef CV_WEAPON_H
#define CV_WEAPON_H
#include "../include/vr_pose.h"
/* Original rigid-pose helpers, unit active xyzw quaternions. */
CvPose cvweapon_compose(CvPose parent,CvPose local);
CvPose cvweapon_inverse(CvPose pose);
/* Compute model-space delta that places original animated dominant wrist at
 * tracked grip position and aligns the actual animated muzzle's +X barrel
 * axis with the absolute controller aim orientation. Preserves gun animation.
 * Inputs/outputs are same original model-space coordinates, never headset UV. */
int cvweapon_attach(const CvPose *animated_wrist,const CvPose *animated_muzzle,
    CvVec3 tracked_grip_position,CvQuat tracked_aim_orientation,CvPose *delta);
/* Verified SP1.3 playerstate+0xb4: reload/start/interrupt/end states 5..9.
 * Freeze the pre-reload attachment reference so controller attachment does not
 * cancel native animated gun/hand motion on every reload frame. */
typedef struct { CvPose wrist,muzzle; unsigned valid; } CvReloadAttachment;
int cvweapon_reload_reference(CvReloadAttachment *,unsigned weapon_state,
    CvPose animated_wrist,CvPose animated_muzzle,CvPose *wrist,CvPose *muzzle);
typedef struct { int latched; } CvTwoHandState;
/* Grip acquisition <=22cm from original foregrip; release beyond40cm or
 * squeeze<0.35. Squeeze>0.65 required to acquire. Rightroll from single_delta
 * is retained by the shortest swing that aligns both controller positions. */
int cvweapon_twohand(const CvPose *native_right,const CvPose *native_left,
    CvVec3 right,CvVec3 left,float left_squeeze,int left_active,float units_per_meter,
    CvPose single_delta,CvTwoHandState *,CvPose *delta,CvPose *locked_left);
#endif
