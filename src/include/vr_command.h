#ifndef CV_VR_COMMAND_H
#define CV_VR_COMMAND_H
#define CV_COMMAND_MAGIC 0x4356434du
#define CV_COMMAND_VERSION 1u
typedef struct {
    unsigned int magic,version,request,ack;
    char text[512];
} CvCommandTransport;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(CvCommandTransport)==528,"Command transport ABI changed");
#endif
#endif
