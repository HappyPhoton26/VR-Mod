#ifndef CV_PICKUP_MATH_H
#define CV_PICKUP_MATH_H
#include "../include/vr_pose.h"
typedef struct { unsigned down,consumed; } CvPickupGripState;
/* Hysteresis and one pickup per squeeze, including when held before reaching. */
unsigned cv_pickup_grips(CvPickupGripState *,unsigned active,const float grip[2]);
int cv_pickup_hand_world(const CvFrame *,const CvPose *anchor,const float ref[22],unsigned hand,CvVec3 *);
/* Squared distance to an item's world-space bounds; -1 rejects malformed bounds. */
float cv_pickup_distance(CvVec3 hand,CvVec3 minimum,CvVec3 maximum);
#endif
