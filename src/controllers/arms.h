#ifndef COD2VR_ARMS_H
#define COD2VR_ARMS_H
#include "../include/vr_pose.h"
#include <stdint.h>
typedef CvVec3 cod2vr_vec3;
typedef CvQuat cod2vr_quat;
typedef CvPose cod2vr_pose;
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    /* Game-space location of tracking origin and rotation of its axes.
     * Caller supplies engine basis; no guessed CoD2 unit scale is baked in. */
    cod2vr_vec3 origin;
    cod2vr_quat tracking_to_game;
    double units_per_meter;
} cod2vr_space;
typedef struct {
    cod2vr_vec3 shoulder;
    cod2vr_pose wrist_target;
    cod2vr_vec3 elbow_hint; /* game-space point indicating elbow bend side */
    cod2vr_vec3 previous_elbow;
    int previous_valid;
    double upper_length, lower_length; /* measured bind-pose game units */
} cod2vr_arm_input;
typedef struct {
    cod2vr_vec3 shoulder, elbow;
    cod2vr_pose wrist;
    cod2vr_quat upper_swing, lower_swing;
    int reach_clamped;
} cod2vr_arm_output;
/* Returns 0 for invalid/nonfinite input, degenerate rotation, bad scale,
 * tracking loss, or timestamp mismatch. Output remains unchanged on failure.
 * Calibration is grip->wrist in grip-local tracking meters. */
int cod2vr_controller_wrist(const CvTrackedPose *grip,
    int64_t pose_time_ns, int64_t frame_time_ns, const cod2vr_pose *calibration,
    const cod2vr_space *space, cod2vr_pose *wrist);
/* Analytical two-bone positions, clamped to reachable range. Wrist orientation
 * preserves the calibrated controller rotation. Swings rotate canonical +X to
 * each solved segment; skeletal adapter must apply measured bind axes/twist.
 * These swings alone are NOT final game bone local rotations. */
int cod2vr_solve_arm(const cod2vr_arm_input *, cod2vr_arm_output *);
#ifdef __cplusplus
}
#endif
#endif
