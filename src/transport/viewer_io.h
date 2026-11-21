#ifndef CV_VIEWER_IO_H
#define CV_VIEWER_IO_H
#include "../include/vr_transport.h"
#include "../include/vr_input.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
static CvImageTransport *game_images;
static CvPoseTransport *game_pose;
static CvInputTransport *game_input;
static unsigned char *game_snapshot;
static GLuint game_textures[2];
static unsigned int game_sequence;
static unsigned int game_texture_width,game_texture_height;
static unsigned long game_uploads;
static CvFrame game_rendered_pose;
static double game_frame_received_at;
static void *open_map(const char *name,size_t size){
    int fd=open(name,O_RDWR|O_CREAT,0600);
    if(fd<0){perror(name);exit(1);}
    if(ftruncate(fd,(off_t)size)){perror("ftruncate");exit(1);}
    void *map=mmap(0,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);close(fd);
    if(map==MAP_FAILED){perror("mmap");exit(1);}return map;
}
static void transport_init(void){
    game_images=open_map("/home/steamos/cod2-vr-work/transport/frame.bin",CV_IMAGE_MAP_BYTES);
    game_pose=open_map("/home/steamos/cod2-vr-work/transport/pose.bin",sizeof(*game_pose));
    game_input=open_map("/home/steamos/cod2-vr-work/transport/input.bin",sizeof(*game_input));
    game_snapshot=malloc(2u*CV_IMAGE_EYE_BYTES);
    if(!game_snapshot)exit(1);
    /* A stopped/restarted viewer must not display leftover game frames. */
    __atomic_store_n(&game_images->magic,0,__ATOMIC_RELEASE);
    glGenTextures(2,game_textures);
    for(unsigned e=0;e<2;e++){
        glBindTexture(GL_TEXTURE_2D,game_textures[e]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }
}
static void publish_tracking(const CvFrame *frame){
    unsigned int seq=__atomic_load_n(&game_pose->sequence,__ATOMIC_RELAXED);
    seq=(seq+1u)|1u;
    __atomic_store_n(&game_pose->sequence,seq,__ATOMIC_SEQ_CST);
    game_pose->magic=CV_POSE_MAGIC;game_pose->version=CV_TRANSPORT_VERSION;
    memcpy(&game_pose->frame,frame,sizeof(*frame));
    __atomic_store_n(&game_pose->sequence,seq+1u,__ATOMIC_RELEASE);
}
static void publish_game_input(const CvButtonsFrame *frame){
    unsigned int seq=__atomic_load_n(&game_input->sequence,__ATOMIC_RELAXED);
    seq=(seq+1u)|1u;
    __atomic_store_n(&game_input->sequence,seq,__ATOMIC_SEQ_CST);
    game_input->magic=CV_INPUT_MAGIC;game_input->version=CV_INPUT_VERSION;
    memcpy(&game_input->frame,frame,sizeof(*frame));
    __atomic_store_n(&game_input->sequence,seq+1u,__ATOMIC_RELEASE);
}
static void receive_images(void){
    unsigned int seq=__atomic_load_n(&game_images->sequence,__ATOMIC_ACQUIRE);
    if(!seq||seq==game_sequence)return;
    CvImageTransport header;memcpy(&header,game_images,sizeof(header));
    unsigned int ring=header.reserved[4]==1,slot=header.reserved[0],slot_sequence=0;
    if(ring){
        if(slot>=CV_IMAGE_RING_SLOTS)return;
        slot_sequence=__atomic_load_n(&game_images->reserved[1+slot],__ATOMIC_ACQUIRE);
        if(!slot_sequence||(slot_sequence&1u))return;
    }else if(seq&1u)return;
    if(header.magic!=CV_IMAGE_MAGIC||header.version!=CV_TRANSPORT_VERSION||
       header.pixel_format!=CV_PIXEL_BGRA8||!header.width||!header.height||
       header.width>CV_MAX_IMAGE_WIDTH||header.height>CV_MAX_IMAGE_HEIGHT||
       header.stride!=header.width*4u||header.eye_stride!=CV_IMAGE_EYE_BYTES)return;
    size_t bytes=(size_t)header.stride*header.height;
    CvFrame rendered_pose={0};
    int have_rendered_pose=ring&&header.reserved[5]==1;
    if(have_rendered_pose){
        const CvFrame *poses=(const CvFrame *)((const unsigned char *)game_images+CV_IMAGE_POSE_OFFSET);
        memcpy(&rendered_pose,&poses[slot],sizeof(rendered_pose));
        have_rendered_pose=rendered_pose.version==CV_FRAME_VERSION&&rendered_pose.size==sizeof(rendered_pose)&&
            (rendered_pose.eyes[0].valid&3u)==3u&&(rendered_pose.eyes[1].valid&3u)==3u;
    }
    if(!have_rendered_pose)return;
    const unsigned char *pixels=(const unsigned char *)game_images+sizeof(header);
    if(ring)pixels+=(size_t)slot*2u*CV_IMAGE_EYE_BYTES;
    for(unsigned e=0;e<2;e++)memcpy(game_snapshot+e*CV_IMAGE_EYE_BYTES,pixels+e*header.eye_stride,bytes);
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    if(ring){
        if(slot_sequence!=__atomic_load_n(&game_images->reserved[1+slot],__ATOMIC_ACQUIRE))return;
    }else if(seq!=__atomic_load_n(&game_images->sequence,__ATOMIC_ACQUIRE))return;
    for(unsigned e=0;e<2;e++){
        glBindTexture(GL_TEXTURE_2D,game_textures[e]);
        if(game_texture_width!=header.width||game_texture_height!=header.height)
            glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,header.width,header.height,0,GL_BGRA,GL_UNSIGNED_BYTE,game_snapshot+e*CV_IMAGE_EYE_BYTES);
        else glTexSubImage2D(GL_TEXTURE_2D,0,0,0,header.width,header.height,GL_BGRA,GL_UNSIGNED_BYTE,game_snapshot+e*CV_IMAGE_EYE_BYTES);
    }
    game_texture_width=header.width;game_texture_height=header.height;
    game_sequence=seq;game_uploads++;
    game_rendered_pose=rendered_pose;game_frame_received_at=clock_s();
}
static int game_world_fresh(void){
    return game_uploads&&clock_s()-game_frame_received_at<.35;
}
static XrPosef rendered_eye_pose(unsigned eye){
    CvPose p=game_rendered_pose.eyes[eye].pose;
    return (XrPosef){{p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w},
                     {p.position.x,p.position.y,p.position.z}};
}
static XrFovf rendered_eye_fov(unsigned eye){
    CvFov f=game_rendered_pose.fov[eye];
    return (XrFovf){f.angle_left,f.angle_right,f.angle_up,f.angle_down};
}
static void draw_game_eye(unsigned int eye){
    if(!game_uploads)return;
    glDisable(GL_DEPTH_TEST);glMatrixMode(GL_PROJECTION);glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,game_textures[eye]);glColor4f(1,1,1,1);
    glBegin(GL_QUADS);
    glTexCoord2f(0,1);glVertex2f(-1,-1);glTexCoord2f(1,1);glVertex2f(1,-1);
    glTexCoord2f(1,0);glVertex2f(1,1);glTexCoord2f(0,0);glVertex2f(-1,1);
    glEnd();glDisable(GL_TEXTURE_2D);
}
static void transport_close(void){
    __atomic_store_n(&game_input->magic,0,__ATOMIC_RELEASE);
    munmap(game_input,sizeof(*game_input));
    __atomic_store_n(&game_pose->magic,0,__ATOMIC_RELEASE);
    munmap(game_images,CV_IMAGE_MAP_BYTES);munmap(game_pose,sizeof(*game_pose));
    free(game_snapshot);glDeleteTextures(2,game_textures);
}
#endif
