#ifndef CV_VR_INPUT_H
#define CV_VR_INPUT_H
#define CV_INPUT_VERSION 1u
#define CV_INPUT_MAGIC 0x4356494eu
#define CV_INPUT_LEFT_ACTIVE 1u
#define CV_INPUT_RIGHT_ACTIVE 2u
#define CV_BUTTON_A 1u
#define CV_BUTTON_B 2u
#define CV_BUTTON_X 4u
#define CV_BUTTON_Y 8u
#define CV_BUTTON_MENU 16u
#define CV_BUTTON_LEFT_STICK 32u
#define CV_BUTTON_RIGHT_STICK 64u
#define CV_BUTTON_DPAD_UP 128u
#define CV_BUTTON_DPAD_DOWN 256u
#define CV_BUTTON_DPAD_LEFT 512u
#define CV_BUTTON_DPAD_RIGHT 1024u
#define CV_BUTTON_RIGHT_BUMPER 2048u
#define CV_BUTTON_LEFT_BUMPER 4096u
/* Separate transport preserves the existing 280-byte tracking frame ABI. */
typedef struct {
    unsigned int version,size;
    unsigned long long sequence;
    long long predicted_time_ns;
    unsigned int active_mask,buttons;
    float stick[2][2],trigger[2],grip[2];
} CvButtonsFrame;
typedef struct {
    unsigned int magic,version,sequence,reserved;
    CvButtonsFrame frame;
} CvInputTransport;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(CvButtonsFrame)==64,"Input frame ABI changed");
_Static_assert(sizeof(CvInputTransport)==80,"Input map ABI changed");
#endif
#endif
