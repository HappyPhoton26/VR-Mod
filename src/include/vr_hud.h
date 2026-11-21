#ifndef CV_VR_HUD_H
#define CV_VR_HUD_H
#define CV_HUD_MAGIC 0x43564844u
#define CV_HUD_VERSION 1u
#define CV_HUD_WIDTH 640u
#define CV_HUD_HEIGHT 360u
#define CV_HUD_SLOT_BYTES (CV_HUD_WIDTH*CV_HUD_HEIGHT*4u)
#define CV_HUD_MAP_BYTES (64u+3u*CV_HUD_SLOT_BYTES)
/* BGRA8 straight-alpha original-game HUD difference layer, top row first. */
typedef struct {
    unsigned int magic,version,sequence,width,height,stride,slot;
    unsigned int generation[3],reserved[6];
} CvHudTransport;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(CvHudTransport)==64,"HUD transport ABI changed");
#endif
#endif
