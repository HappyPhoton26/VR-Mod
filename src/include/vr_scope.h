#ifndef CV_VR_SCOPE_H
#define CV_VR_SCOPE_H
#define CV_SCOPE_MAGIC 0x43565343u
#define CV_SCOPE_VERSION 1u
#define CV_SCOPE_SIZE 640u
#define CV_SCOPE_SLOT_BYTES (CV_SCOPE_SIZE*CV_SCOPE_SIZE*4u)
#define CV_SCOPE_MAP_BYTES (64u+3u*CV_SCOPE_SLOT_BYTES)
/* Compact independent, monoscopic magnified rifle view. BGRA8 top-down.
 * reserved[0]=active; per-slot generation protects pixels. No world-eye ABI change. */
typedef struct {
 unsigned magic,version,sequence,width,height,stride,slot;
 unsigned generation[3],reserved[6];
} CvScopeTransport;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(CvScopeTransport)==64,"Scope transport ABI changed");
#endif
#endif
