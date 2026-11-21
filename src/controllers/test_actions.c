#include "actions.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define HANDLE(type,n) ((type)(uintptr_t)(n))
static int destroyed_spaces,destroyed_sets,attached,locates,inactive,unfocused;
static XrTime expected_time=123456789;
static XrResult XRAPI_PTR stringpath(XrInstance i,const char *s,XrPath *p){(void)i;*p=strstr(s,"left")?1:2;return XR_SUCCESS;}
static XrResult XRAPI_PTR createset(XrInstance i,const XrActionSetCreateInfo *c,XrActionSet *s){(void)i;assert(c->type==XR_TYPE_ACTION_SET_CREATE_INFO);*s=HANDLE(XrActionSet,10);return XR_SUCCESS;}
static XrResult XRAPI_PTR destroyset(XrActionSet s){(void)s;destroyed_sets++;return XR_SUCCESS;}
static XrResult XRAPI_PTR createaction(XrActionSet s,const XrActionCreateInfo *c,XrAction *a){(void)s;assert(c->countSubactionPaths==2);const char *names[]={"grip_pose","aim_pose","move_stick","fire_trigger","hold_grip","button_a","button_b","button_x","button_y","game_menu","stick_click","dpad_up","dpad_down","dpad_left","dpad_right","reload_bumper"};for(unsigned i=0;i<16;i++)if(!strcmp(names[i],c->actionName)){*a=HANDLE(XrAction,20+i);return XR_SUCCESS;}return XR_ERROR_NAME_INVALID;}
static XrResult XRAPI_PTR createspace(XrSession s,const XrActionSpaceCreateInfo *c,XrSpace *p){(void)s;assert(c->poseInActionSpace.orientation.w==1);*p=HANDLE(XrSpace,(uintptr_t)c->action+c->subactionPath);return XR_SUCCESS;}
static XrResult XRAPI_PTR destroyspace(XrSpace s){(void)s;destroyed_spaces++;return XR_SUCCESS;}
static XrResult XRAPI_PTR suggest(XrInstance i,const XrInteractionProfileSuggestedBinding *b){(void)i;assert(b->countSuggestedBindings>=4&&b->countSuggestedBindings<=36);return XR_SUCCESS;}
static XrResult XRAPI_PTR attach(XrSession s,const XrSessionActionSetsAttachInfo *a){(void)s;assert(a->countActionSets==1);attached++;return XR_SUCCESS;}
static XrResult XRAPI_PTR syncactions(XrSession s,const XrActionsSyncInfo *a){(void)s;assert(a->countActiveActionSets==1);return unfocused?XR_SESSION_NOT_FOCUSED:XR_SUCCESS;}
static XrResult XRAPI_PTR statepose(XrSession s,const XrActionStateGetInfo *g,XrActionStatePose *o){(void)s;(void)g;o->isActive=!inactive;return XR_SUCCESS;}
static XrResult XRAPI_PTR statefloat(XrSession s,const XrActionStateGetInfo *g,XrActionStateFloat *o){(void)s;o->isActive=!inactive;o->currentState=(uintptr_t)g->action==23?.75f:.25f;return XR_SUCCESS;}
static XrResult XRAPI_PTR stateboolean(XrSession s,const XrActionStateGetInfo *g,XrActionStateBoolean *o){(void)s;o->isActive=!inactive;uintptr_t a=(uintptr_t)g->action;o->currentState=(g->subactionPath==2&&(a==25||a==28||a==35))||(g->subactionPath==1&&(a==27||a==35));return XR_SUCCESS;}
static XrResult XRAPI_PTR statevector(XrSession s,const XrActionStateGetInfo *g,XrActionStateVector2f *o){(void)s;(void)g;o->isActive=!inactive;o->currentState=(XrVector2f){.2f,.8f};return XR_SUCCESS;}
static XrResult XRAPI_PTR locate(XrSpace s,XrSpace b,XrTime t,XrSpaceLocation *o){(void)s;(void)b;assert(t==expected_time);locates++;o->locationFlags=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;o->pose.position=(XrVector3f){1,2,3};o->pose.orientation=(XrQuaternionf){0,0,0,1};return XR_SUCCESS;}
static XrResult XRAPI_PTR resolver(XrInstance i,const char *n,PFN_xrVoidFunction *f){
    (void)i;
#define RESOLVE(name,fn) if(!strcmp(n,"xr" name)){*f=(PFN_xrVoidFunction)fn;return XR_SUCCESS;}
    RESOLVE("StringToPath",stringpath) RESOLVE("CreateActionSet",createset)
    RESOLVE("DestroyActionSet",destroyset) RESOLVE("CreateAction",createaction)
    RESOLVE("CreateActionSpace",createspace) RESOLVE("DestroySpace",destroyspace)
    RESOLVE("SuggestInteractionProfileBindings",suggest) RESOLVE("AttachSessionActionSets",attach)
    RESOLVE("SyncActions",syncactions) RESOLVE("GetActionStatePose",statepose) RESOLVE("LocateSpace",locate)
    RESOLVE("GetActionStateFloat",statefloat) RESOLVE("GetActionStateBoolean",stateboolean) RESOLVE("GetActionStateVector2f",statevector)
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}
int main(void){
    assert(cvcontrols_init(HANDLE(XrInstance,1),HANDLE(XrSession,2),HANDLE(XrSpace,3),resolver)==XR_SUCCESS);
    assert(attached==1);
    CvFrame f={.version=CV_FRAME_VERSION,.size=sizeof f,.predicted_display_time_ns=123456789};
    assert(cvcontrols_update(&f)==XR_SUCCESS);assert(locates==4);
    CvButtonsFrame buttons;cvcontrols_buttons(&buttons);assert(buttons.version==1&&buttons.size==64&&buttons.predicted_time_ns==expected_time);
    assert(buttons.active_mask==3&&buttons.trigger[1]==.75f&&buttons.stick[0][1]==.8f);
    assert(buttons.buttons==(CV_BUTTON_A|CV_BUTTON_X|CV_BUTTON_Y|CV_BUTTON_RIGHT_BUMPER|CV_BUTTON_LEFT_BUMPER));
    for(int h=0;h<2;h++){assert(f.grip[h].valid==3);assert(f.aim[h].valid==3);assert(f.grip[h].pose.position.z==3);}
    inactive=1;assert(cvcontrols_update(&f)==XR_SUCCESS);assert(locates==4);
    for(int h=0;h<2;h++){assert(f.grip[h].valid==0);assert(f.aim[h].valid==0);}
    cvcontrols_buttons(&buttons);assert(!buttons.active_mask&&!buttons.buttons&&!buttons.trigger[1]);
    inactive=0;assert(cvcontrols_update(&f)==XR_SUCCESS);
    unfocused=1;assert(cvcontrols_update(&f)==XR_SESSION_NOT_FOCUSED);assert(!f.grip[0].valid&&!f.aim[1].valid);
    cvcontrols_buttons(&buttons);assert(!buttons.active_mask&&!buttons.buttons&&!buttons.trigger[1]);
    cvcontrols_close();assert(destroyed_spaces==4&&destroyed_sets==1);
    cvcontrols_close();assert(destroyed_spaces==4&&destroyed_sets==1);
    assert(cvcontrols_update(&f)==XR_ERROR_VALIDATION_FAILURE);
    puts("PASS: action creation/attach, same-time pose sampling, inactive/focus clearing, cleanup");
}
