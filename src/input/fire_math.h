#ifndef CV_FIRE_MATH_H
#define CV_FIRE_MATH_H
#include "../include/vr_pose.h"
/* Returns1 only after replacing the twelve forward/right/up/muzzle floats. */
int cv_fire_apply(float *,unsigned int,unsigned int,const CvPose *,unsigned long long,long long,const CvFrame *,const float *);
#endif
