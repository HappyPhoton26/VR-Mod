/* Blend the native HUD and game menus into the eye images. */
#include "../include/vr_hud.h"
static CvHudTransport *game_hud;
static unsigned char hud_snapshot_pixels[CV_HUD_SLOT_BYTES];
static GLuint hud_texture;
static unsigned int hud_sequence,hud_menu,hud_ready,hud_visible_pixels;
static double hud_received_at;
static void hud_viewer_init(void){
    game_hud=open_map("/home/steamos/cod2-vr-work/transport/hud.bin",CV_HUD_MAP_BYTES);
    __atomic_store_n(&game_hud->magic,0,__ATOMIC_RELEASE);
    glGenTextures(1,&hud_texture);glBindTexture(GL_TEXTURE_2D,hud_texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,CV_HUD_WIDTH,CV_HUD_HEIGHT,0,GL_BGRA,GL_UNSIGNED_BYTE,0);
}
static void receive_hud(void){
    unsigned int sequence=__atomic_load_n(&game_hud->sequence,__ATOMIC_ACQUIRE);
    if(!sequence||sequence==hud_sequence)return;
    CvHudTransport header;memcpy(&header,game_hud,sizeof(header));
    if(header.magic!=CV_HUD_MAGIC||header.version!=CV_HUD_VERSION||header.slot>2||
       header.width!=CV_HUD_WIDTH||header.height!=CV_HUD_HEIGHT||header.stride!=CV_HUD_WIDTH*4u)return;
    unsigned int generation=__atomic_load_n(&game_hud->generation[header.slot],__ATOMIC_ACQUIRE);
    if(!generation||(generation&1u))return;
    memcpy(hud_snapshot_pixels,(const unsigned char *)game_hud+64+header.slot*CV_HUD_SLOT_BYTES,CV_HUD_SLOT_BYTES);
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    if(generation!=__atomic_load_n(&game_hud->generation[header.slot],__ATOMIC_ACQUIRE))return;
    glBindTexture(GL_TEXTURE_2D,hud_texture);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,CV_HUD_WIDTH,CV_HUD_HEIGHT,GL_BGRA,GL_UNSIGNED_BYTE,hud_snapshot_pixels);
    hud_menu=header.reserved[0];hud_visible_pixels=header.reserved[1];hud_sequence=sequence;hud_ready=1;hud_received_at=clock_s();
}
static int hud_fresh(void){
    return hud_ready&&hud_visible_pixels&&clock_s()-hud_received_at<.35;
}
static void draw_hud_eye(XrFovf fov){
    if(!hud_fresh())return;
    float distance=hud_menu?1.8f:1.5f;
    float half_width=(hud_menu?1.65f:1.28f)*.5f/distance;
    float half_height=(hud_menu?.928125f:.72f)*.5f/distance;
    float left=tanf(fov.angleLeft),right=tanf(fov.angleRight);
    float down=tanf(fov.angleDown),up=tanf(fov.angleUp);
    if(!(right>left&&up>down))return;
    float x0=2*(-half_width-left)/(right-left)-1,x1=2*(half_width-left)/(right-left)-1;
    float y0=2*(-half_height-down)/(up-down)-1,y1=2*(half_height-down)/(up-down)-1;
    glDisable(GL_DEPTH_TEST);glMatrixMode(GL_PROJECTION);glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,hud_texture);glColor4f(1,1,1,1);
    glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    glTexCoord2f(0,1);glVertex2f(x0,y0);glTexCoord2f(1,1);glVertex2f(x1,y0);
    glTexCoord2f(1,0);glVertex2f(x1,y1);glTexCoord2f(0,0);glVertex2f(x0,y1);
    glEnd();glDisable(GL_BLEND);glDisable(GL_TEXTURE_2D);
}
static void hud_viewer_close(void){
    munmap(game_hud,CV_HUD_MAP_BYTES);glDeleteTextures(1,&hud_texture);
}
