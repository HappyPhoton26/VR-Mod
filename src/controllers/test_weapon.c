#include "weapon.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static double distance(CvVec3 a,CvVec3 b){double x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return sqrt(x*x+y*y+z*z);}
static CvVec3 axis_x(CvPose pose){CvPose p={.orientation={0,0,0,1},.position={1,0,0}};CvVec3 out=cvweapon_compose(pose,p).position;out.x-=pose.position.x;out.y-=pose.position.y;out.z-=pose.position.z;return out;}
int main(void){
    CvPose wrist={.orientation={0,0,0,1},.position={4,-5,-5}};
    CvPose muzzle={.orientation={0,0,.38268343f,.92387953f},.position={26,29,-9}};
    CvQuat aim={0,.70710678f,0,.70710678f};CvVec3 grip={10,-12,-8};CvPose delta;
    assert(cvweapon_attach(&wrist,&muzzle,grip,aim,&delta));
    assert(distance(cvweapon_compose(delta,wrist).position,grip)<1e-5);
    CvPose desired_aim={.orientation=aim,.position={0,0,0}};
    assert(distance(axis_x(cvweapon_compose(delta,muzzle)),axis_x(desired_aim))<1e-5);
    CvPose original_entity={.orientation={0,0,.70710678f,.70710678f},.position={100,200,30}};
    CvPose driven_entity=cvweapon_compose(original_entity,delta);
    CvPose shoulder={.orientation={.5f,.5f,.5f,.5f},.position={0,6,-9}};
    CvPose body_relative=cvweapon_compose(cvweapon_inverse(delta),shoulder);
    CvPose unchanged_world=cvweapon_compose(original_entity,shoulder),after=cvweapon_compose(driven_entity,body_relative);
    assert(distance(unchanged_world.position,after.position)<4e-5);
    assert(distance(axis_x(unchanged_world),axis_x(after))<3e-5);
    assert(distance(cvweapon_compose(driven_entity,wrist).position,cvweapon_compose(original_entity,(CvPose){.orientation={0,0,0,1},.position=grip}).position)<3e-5);
    CvPose identity=cvweapon_compose(cvweapon_inverse(driven_entity),driven_entity);
    assert(distance(identity.position,(CvVec3){0,0,0})<4e-5);assert(fabs(identity.orientation.w-1)<1e-5);
    CvPose bad=muzzle;bad.orientation=(CvQuat){0,0,0,0};assert(!cvweapon_attach(&wrist,&bad,grip,aim,&delta));
    bad=muzzle;bad.position.x=NAN;assert(!cvweapon_attach(&wrist,&bad,grip,aim,&delta));
    CvPose nr={.orientation={0,0,0,1},.position={0,0,0}},nl={.orientation={0,0,0,1},.position={12,0,0}},locked;
    CvPose single={.orientation={.38268343f,0,0,.92387953f},.position={5,2,-3}};
    CvTwoHandState two={0};CvVec3 right={5,2,-3},left={17,2,-3};
    assert(cvweapon_twohand(&nr,&nl,right,left,0,1,40,single,&two,&delta,&locked));assert(!two.latched);
    assert(cvweapon_twohand(&nr,&nl,right,left,1,1,40,single,&two,&delta,&locked));assert(two.latched);
    left=(CvVec3){5,14,-3};
    assert(cvweapon_twohand(&nr,&nl,right,left,.5f,1,40,single,&two,&delta,&locked));assert(two.latched);
    assert(distance(locked.position,left)<1e-5);assert(distance(cvweapon_compose(delta,nr).position,right)<1e-5);
    assert(distance(axis_x(delta),(CvVec3){0,1,0})<1e-5);
    /* Shortest swing preserves right-hand roll; local up points partly X/Z. */
    CvPose up={.orientation={0,0,0,1},.position={0,0,1}};CvVec3 roll=cvweapon_compose(delta,up).position;
    roll.x-=delta.position.x;roll.y-=delta.position.y;roll.z-=delta.position.z;
    assert(distance(roll,(CvVec3){.70710678f,0,.70710678f})<1e-5);
    assert(cvweapon_twohand(&nr,&nl,right,left,.2f,1,40,single,&two,&delta,&locked));assert(!two.latched);
    left=(CvVec3){17,2,-3};assert(cvweapon_twohand(&nr,&nl,right,left,1,1,40,single,&two,&delta,&locked));assert(two.latched);
    left=(CvVec3){50,2,-3};assert(cvweapon_twohand(&nr,&nl,right,left,1,1,40,single,&two,&delta,&locked));assert(!two.latched);
    left=(CvVec3){17,2,-3};assert(cvweapon_twohand(&nr,&nl,right,left,1,1,40,single,&two,&delta,&locked));assert(two.latched);
    assert(cvweapon_twohand(&nr,&nl,right,left,1,0,40,single,&two,&delta,&locked));assert(!two.latched);
    CvReloadAttachment reload={0};CvPose reference_wrist,reference_muzzle;
    assert(!cvweapon_reload_reference(&reload,0,wrist,muzzle,&reference_wrist,&reference_muzzle));
    CvPose animated_wrist=wrist,animated_muzzle=muzzle;
    animated_wrist.position.x+=8;animated_muzzle.orientation=(CvQuat){0,0,.70710678f,.70710678f};
    for(unsigned st=5;st<=9;st++){
        assert(cvweapon_reload_reference(&reload,st,animated_wrist,animated_muzzle,&reference_wrist,&reference_muzzle));
        assert(distance(reference_wrist.position,wrist.position)<1e-6);
        assert(fabs(reference_muzzle.orientation.w-muzzle.orientation.w)<1e-6);
        CvPose animated_delta;
        assert(cvweapon_attach(&reference_wrist,&reference_muzzle,grip,aim,&delta));
        assert(cvweapon_attach(&animated_wrist,&animated_muzzle,grip,aim,&animated_delta));
        /* Preserved reload animation moves relative to the grip. The old
         * per-frame attachment instead canceled it to the controller. */
        assert(distance(cvweapon_compose(delta,animated_wrist).position,
                        cvweapon_compose(animated_delta,animated_wrist).position)>1);
    }
    assert(!cvweapon_reload_reference(&reload,0,animated_wrist,animated_muzzle,&reference_wrist,&reference_muzzle));
    assert(distance(reference_wrist.position,animated_wrist.position)<1e-6);
    for(unsigned st=10;st<=22;st++)assert(!cvweapon_reload_reference(&reload,st,wrist,muzzle,&reference_wrist,&reference_muzzle));
    puts("PASS: weapon6DoF grip translation, absolutebarrel aim, originalbody worldanchor, rigidinverse guards");
}
