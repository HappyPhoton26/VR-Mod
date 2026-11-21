#include "actions.h"
#include <string.h>
#include <stdio.h>
static XrInstance inst;
static XrSession session;
static XrSpace base,spaces[2][2];
static XrPath hands[2];
static XrActionSet set;
static XrAction poses[2];
enum {STICK,TRIGGER,SQUEEZE,BUTTON_A,BUTTON_B,BUTTON_X,BUTTON_Y,MENU,STICK_CLICK,DPAD_UP,DPAD_DOWN,DPAD_LEFT,DPAD_RIGHT,BUMPER,EXTRA_COUNT};
static XrAction extra[EXTRA_COUNT];
static CvButtonsFrame buttons;
static unsigned long long buttons_sequence;
#define FUNCS(X) X(StringToPath) X(CreateActionSet) X(DestroyActionSet) X(CreateAction) X(CreateActionSpace) X(DestroySpace) X(SuggestInteractionProfileBindings) X(AttachSessionActionSets) X(SyncActions) X(GetActionStatePose) X(LocateSpace) X(GetActionStateFloat) X(GetActionStateBoolean) X(GetActionStateVector2f)
#define DECL(n) static PFN_xr##n p##n;
FUNCS(DECL)
#define TRY(e) do {XrResult r=(e);if(XR_FAILED(r)){cvcontrols_close();return r;}}while(0)
XrResult cvcontrols_init(XrInstance i,XrSession s,XrSpace b,PFN_xrGetInstanceProcAddr resolver){
    if(!i||!s||!b||!resolver||set)return XR_ERROR_VALIDATION_FAILURE;
    inst=i;session=s;base=b;
#define LOAD(n) TRY(resolver(inst,"xr" #n,(PFN_xrVoidFunction *)&p##n));
    FUNCS(LOAD)
#undef LOAD
    TRY(pStringToPath(inst,"/user/hand/left",&hands[0]));
    TRY(pStringToPath(inst,"/user/hand/right",&hands[1]));
    XrActionSetCreateInfo ci={.type=XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy(ci.actionSetName,"cod2vr_hands");strcpy(ci.localizedActionSetName,"CoD2 VR hands");
    TRY(pCreateActionSet(inst,&ci,&set));
    const char *names[]={"grip_pose","aim_pose"};
    for(int k=0;k<2;k++){
        XrActionCreateInfo ai={.type=XR_TYPE_ACTION_CREATE_INFO,.actionType=XR_ACTION_TYPE_POSE_INPUT,.countSubactionPaths=2,.subactionPaths=hands};
        strcpy(ai.actionName,names[k]);strcpy(ai.localizedActionName,names[k]);
        TRY(pCreateAction(set,&ai,&poses[k]));
        for(int h=0;h<2;h++){
            XrActionSpaceCreateInfo si={.type=XR_TYPE_ACTION_SPACE_CREATE_INFO,.action=poses[k],.subactionPath=hands[h],.poseInActionSpace={{0,0,0,1},{0,0,0}}};
            TRY(pCreateActionSpace(session,&si,&spaces[k][h]));
        }
    }
    const char *extra_names[]={"move_stick","fire_trigger","hold_grip","button_a","button_b","button_x","button_y","game_menu","stick_click","dpad_up","dpad_down","dpad_left","dpad_right","reload_bumper"};
    for(int k=0;k<EXTRA_COUNT;k++){
        XrActionCreateInfo ai={.type=XR_TYPE_ACTION_CREATE_INFO,.countSubactionPaths=2,.subactionPaths=hands};
        ai.actionType=k==STICK?XR_ACTION_TYPE_VECTOR2F_INPUT:(k==TRIGGER||k==SQUEEZE?XR_ACTION_TYPE_FLOAT_INPUT:XR_ACTION_TYPE_BOOLEAN_INPUT);
        strcpy(ai.actionName,extra_names[k]);strcpy(ai.localizedActionName,extra_names[k]);
        TRY(pCreateAction(set,&ai,&extra[k]));
    }
    /* All use standard grip/aim pose paths. Unsupported optional profiles are
       allowed; at least simple-controller must be accepted by the runtime. */
    const char *profiles[]={"/interaction_profiles/khr/simple_controller","/interaction_profiles/valve/index_controller","/interaction_profiles/oculus/touch_controller","/interaction_profiles/valve/frame_controller"};
    int accepted=0;
    for(unsigned p=0;p<sizeof profiles/sizeof profiles[0];p++){
        XrActionSuggestedBinding binds[64];unsigned n=0;
        for(int k=0;k<2;k++)for(int h=0;h<2;h++){
            char path[100];snprintf(path,sizeof path,"/user/hand/%s/input/%s/pose",h?"right":"left",k?"aim":"grip");
            XrPath binding;TRY(pStringToPath(inst,path,&binding));
            binds[n++]=(XrActionSuggestedBinding){poses[k],binding};
        }
        XrPath profile;TRY(pStringToPath(inst,profiles[p],&profile));
        XrInteractionProfileSuggestedBinding bi={.type=XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,.interactionProfile=profile,.countSuggestedBindings=n,.suggestedBindings=binds};
        XrResult r=pSuggestInteractionProfileBindings(inst,&bi);
        if(XR_SUCCEEDED(r)){
            accepted++;
            /* Logical face-button actions must stay distinct: Frame has ALL
             * A/B/X/Y on the right, unlike Touch's left X/Y. */
            for(int h=0;h<2;h++){
                struct {int action;const char *path;} optional[]={
                    {STICK,"thumbstick"},{TRIGGER,"trigger/value"},{SQUEEZE,"squeeze/value"},{SQUEEZE,"grip/value"},
                    {BUTTON_A,"a/click"},{BUTTON_B,"b/click"},{BUTTON_X,"x/click"},{BUTTON_Y,"y/click"},
                    {BUMPER,"bumper/click"},{MENU,"menu/click"},{MENU,"view/click"},{STICK_CLICK,"thumbstick/click"},
                    {DPAD_UP,"dpad_up/click"},{DPAD_DOWN,"dpad_down/click"},{DPAD_LEFT,"dpad_left/click"},{DPAD_RIGHT,"dpad_right/click"}};
                for(unsigned k=0;k<sizeof optional/sizeof optional[0];k++){
                    char path[100];snprintf(path,sizeof path,"/user/hand/%s/input/%s",h?"right":"left",optional[k].path);
                    XrPath binding;if(XR_FAILED(pStringToPath(inst,path,&binding)))continue;
                    binds[n]=(XrActionSuggestedBinding){extra[optional[k].action],binding};
                    bi.countSuggestedBindings=n+1;
                    if(XR_SUCCEEDED(pSuggestInteractionProfileBindings(inst,&bi)))n++;
                }
            }
            bi.countSuggestedBindings=n;
            TRY(pSuggestInteractionProfileBindings(inst,&bi));
            printf("Gameplay bindings %s: %u\n",profiles[p],n);
        }
    }
    if(!accepted){cvcontrols_close();return XR_ERROR_PATH_UNSUPPORTED;}
    XrSessionActionSetsAttachInfo attach={.type=XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,.countActionSets=1,.actionSets=&set};
    TRY(pAttachSessionActionSets(session,&attach));
    return XR_SUCCESS;
}
XrResult cvcontrols_update(CvFrame *f){
    if(!f||!set||f->version!=CV_FRAME_VERSION||f->size!=sizeof *f)return XR_ERROR_VALIDATION_FAILURE;
    memset(f->grip,0,sizeof f->grip);memset(f->aim,0,sizeof f->aim);
    memset(&buttons,0,sizeof buttons);buttons.version=CV_INPUT_VERSION;buttons.size=sizeof buttons;
    buttons.sequence=++buttons_sequence;buttons.predicted_time_ns=f->predicted_display_time_ns;
    XrActiveActionSet active={set,XR_NULL_PATH};
    XrActionsSyncInfo sync={.type=XR_TYPE_ACTIONS_SYNC_INFO,.countActiveActionSets=1,.activeActionSets=&active};
    XrResult r=pSyncActions(session,&sync);if(XR_FAILED(r)||r==XR_SESSION_NOT_FOCUSED)return r;
    for(int k=0;k<2;k++)for(int h=0;h<2;h++){
        XrActionStateGetInfo gi={.type=XR_TYPE_ACTION_STATE_GET_INFO,.action=poses[k],.subactionPath=hands[h]};
        XrActionStatePose state={.type=XR_TYPE_ACTION_STATE_POSE};
        r=pGetActionStatePose(session,&gi,&state);if(XR_FAILED(r))return r;
        if(!state.isActive)continue;
        buttons.active_mask|=1u<<h;
        XrSpaceLocation loc={.type=XR_TYPE_SPACE_LOCATION};
        r=pLocateSpace(spaces[k][h],base,f->predicted_display_time_ns,&loc);if(XR_FAILED(r))return r;
        CvTrackedPose *o=k?&f->aim[h]:&f->grip[h];
        if(loc.locationFlags&XR_SPACE_LOCATION_ORIENTATION_VALID_BIT){
            o->pose.orientation=(CvQuat){loc.pose.orientation.x,loc.pose.orientation.y,loc.pose.orientation.z,loc.pose.orientation.w};o->valid|=CV_POSE_ORIENTATION_VALID;
        }
        if(loc.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT){
            o->pose.position=(CvVec3){loc.pose.position.x,loc.pose.position.y,loc.pose.position.z};o->valid|=CV_POSE_POSITION_VALID;
        }
    }
    for(int h=0;h<2;h++)for(int k=0;k<EXTRA_COUNT;k++){
        XrActionStateGetInfo gi={.type=XR_TYPE_ACTION_STATE_GET_INFO,.action=extra[k],.subactionPath=hands[h]};
        if(k==STICK){
            XrActionStateVector2f value={.type=XR_TYPE_ACTION_STATE_VECTOR2F};
            r=pGetActionStateVector2f(session,&gi,&value);if(XR_FAILED(r))return r;
            if(value.isActive){buttons.active_mask|=1u<<h;buttons.stick[h][0]=value.currentState.x;buttons.stick[h][1]=value.currentState.y;}
        }else if(k==TRIGGER||k==SQUEEZE){
            XrActionStateFloat value={.type=XR_TYPE_ACTION_STATE_FLOAT};
            r=pGetActionStateFloat(session,&gi,&value);if(XR_FAILED(r))return r;
            if(value.isActive){buttons.active_mask|=1u<<h;if(k==TRIGGER)buttons.trigger[h]=value.currentState;else buttons.grip[h]=value.currentState;}
        }else{
            XrActionStateBoolean value={.type=XR_TYPE_ACTION_STATE_BOOLEAN};
            r=pGetActionStateBoolean(session,&gi,&value);if(XR_FAILED(r))return r;
            if(value.isActive){
                buttons.active_mask|=1u<<h;
                if(value.currentState){
                    if(k==BUTTON_A)buttons.buttons|=CV_BUTTON_A;
                    if(k==BUTTON_B)buttons.buttons|=CV_BUTTON_B;
                    if(k==BUTTON_X)buttons.buttons|=CV_BUTTON_X;
                    if(k==BUTTON_Y)buttons.buttons|=CV_BUTTON_Y;
                    if(k==BUMPER&&h==CV_HAND_RIGHT)buttons.buttons|=CV_BUTTON_RIGHT_BUMPER;
                    if(k==MENU)buttons.buttons|=CV_BUTTON_MENU;
                    if(k==STICK_CLICK)buttons.buttons|=h?CV_BUTTON_RIGHT_STICK:CV_BUTTON_LEFT_STICK;
                    if(k==DPAD_UP)buttons.buttons|=CV_BUTTON_DPAD_UP;
                    if(k==DPAD_DOWN)buttons.buttons|=CV_BUTTON_DPAD_DOWN;
                    if(k==DPAD_LEFT)buttons.buttons|=CV_BUTTON_DPAD_LEFT;
                    if(k==DPAD_RIGHT)buttons.buttons|=CV_BUTTON_DPAD_RIGHT;
                }
            }
        }
    }
    return XR_SUCCESS;
}
void cvcontrols_buttons(CvButtonsFrame *out){if(out)memcpy(out,&buttons,sizeof buttons);}
void cvcontrols_close(void){
    for(int k=0;k<2;k++)for(int h=0;h<2;h++)if(spaces[k][h]){
        if(pDestroySpace)pDestroySpace(spaces[k][h]);
        spaces[k][h]=XR_NULL_HANDLE;
    }
    if(set&&pDestroyActionSet)pDestroyActionSet(set);
    set=XR_NULL_HANDLE;inst=XR_NULL_HANDLE;session=XR_NULL_HANDLE;base=XR_NULL_HANDLE;
    memset(poses,0,sizeof poses);memset(hands,0,sizeof hands);
    memset(extra,0,sizeof extra);memset(&buttons,0,sizeof buttons);buttons_sequence=0;
}
