#ifndef CV_TRANSPORT_H
#define CV_TRANSPORT_H
#include "vr_pose.h"
#define CV_TRANSPORT_VERSION 1u
#define CV_IMAGE_MAGIC 0x43565246u
#define CV_POSE_MAGIC 0x43565250u
#define CV_PIXEL_BGRA8 1u
#define CV_MAX_IMAGE_WIDTH 5120u
#define CV_MAX_IMAGE_HEIGHT 2880u
#define CV_IMAGE_EYE_BYTES (CV_MAX_IMAGE_WIDTH*CV_MAX_IMAGE_HEIGHT*4u)
#define CV_IMAGE_RING_SLOTS 3u
#define CV_IMAGE_POSE_OFFSET (64u+CV_IMAGE_RING_SLOTS*2u*CV_IMAGE_EYE_BYTES)
#define CV_IMAGE_MAP_BYTES (CV_IMAGE_POSE_OFFSET+CV_IMAGE_RING_SLOTS*(unsigned int)sizeof(CvFrame))
/* Single writer seqlock: increment sequence to odd, publish all fields/pixels,
 * memory fence, increment to even. Reader snapshots only matching even sequence.
 * Pixels begin at byte64; second eye at64+eye_stride. Top row first, BGRA8.
 * Ring extension: reserved[4]==1; reserved[0] publishes the current slot;
 * reserved[1+slot] is that slot's seqlock. A slot begins at
 * byte64+slot*2*CV_IMAGE_EYE_BYTES. Global sequence advances by2 AFTER each
 * publication. Readers validate the SLOT sequence, allowing the producer to
 * write a different slot concurrently. Legacy format has reserved[4]==0.
 * reserved[5]==1 indicates rendered CvFrame metadata appended at
 * CV_IMAGE_POSE_OFFSET, indexed by slot. It is protected by the same slot lock.
 * Fixed mapping path: Z:\home\steamos\cod2-vr-work\transport\frame.bin.
 */
typedef struct {
    unsigned int magic,version,sequence,width,height,stride,eye_stride,pixel_format;
    unsigned int reserved[8];
} CvImageTransport;
/* Native compositor writes predicted eye/head/hand tracking here. Renderer
 * reads stable frame, converts OpenXR metres/axes at the engine boundary.
 * Z:\home\steamos\cod2-vr-work\transport\pose.bin
 */
typedef struct {
    unsigned int magic,version,sequence,reserved;
    CvFrame frame;
} CvPoseTransport;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(CvImageTransport)==64,"Image transport ABI changed");
_Static_assert(sizeof(CvPoseTransport)==296,"Pose transport ABI changed");
#endif
#endif
