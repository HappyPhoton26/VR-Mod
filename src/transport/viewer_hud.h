/* Display the full native HUD and game menus as a head-relative quad. */
#include "../include/vr_hud.h"
static CvHudTransport *game_hud;
static unsigned char hud_snapshot_pixels[CV_HUD_SLOT_BYTES];
static GLuint hud_texture;
static unsigned int hud_sequence,hud_menu,hud_ready,hud_visible_pixels;
static XrSwapchain hud_chain;
static XrSwapchainImageOpenGLKHR hud_images[16];
static GLuint hud_framebuffer;
static unsigned int hud_uploaded_sequence;
static void hud_quad_init(XrSession session,int64_t format){
    XrSwapchainCreateInfo ci={XR_TYPE_SWAPCHAIN_CREATE_INFO};
    ci.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    ci.format=format;ci.sampleCount=1;ci.width=CV_HUD_WIDTH;ci.height=CV_HUD_HEIGHT;
    ci.faceCount=ci.arraySize=ci.mipCount=1;CHECK(xrCreateSwapchain(session,&ci,&hud_chain));
    for(unsigned i=0;i<16;i++)hud_images[i].type=XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
    uint32_t count=0;CHECK(xrEnumerateSwapchainImages(hud_chain,16,&count,(XrSwapchainImageBaseHeader *)hud_images));
    glGenFramebuffers(1,&hud_framebuffer);
}
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
    hud_menu=header.reserved[0];hud_visible_pixels=header.reserved[1];hud_sequence=sequence;hud_ready=1;
}
static void draw_hud_quad_texture(void){
    glDisable(GL_DEPTH_TEST);glMatrixMode(GL_PROJECTION);glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,hud_texture);glColor4f(1,1,1,1);
    glDisable(GL_BLEND);
    glBegin(GL_QUADS);
    glTexCoord2f(0,1);glVertex2f(-1,-1);glTexCoord2f(1,1);glVertex2f(1,-1);
    glTexCoord2f(1,0);glVertex2f(1,1);glTexCoord2f(0,0);glVertex2f(-1,1);
    glEnd();glDisable(GL_BLEND);glDisable(GL_TEXTURE_2D);
}
static int hud_quad_layer(XrSpace head_space,XrCompositionLayerQuad *layer){
    if(!hud_ready||(!hud_menu&&!hud_visible_pixels))return 0;
    if(hud_uploaded_sequence!=hud_sequence){
        uint32_t index;XrSwapchainImageAcquireInfo ai={XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        CHECK(xrAcquireSwapchainImage(hud_chain,&ai,&index));
        XrSwapchainImageWaitInfo wi={XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=XR_INFINITE_DURATION;
        CHECK(xrWaitSwapchainImage(hud_chain,&wi));
        glBindFramebuffer(GL_FRAMEBUFFER,hud_framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,hud_images[index].image,0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){fprintf(stderr,"HUD quad framebuffer incomplete\n");exit(4);}
        glViewport(0,0,CV_HUD_WIDTH,CV_HUD_HEIGHT);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);
        glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);draw_hud_quad_texture();
        glFlush();glBindFramebuffer(GL_FRAMEBUFFER,0);
        XrSwapchainImageReleaseInfo ri={XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};CHECK(xrReleaseSwapchainImage(hud_chain,&ri));
        hud_uploaded_sequence=hud_sequence;
    }
    *layer=(XrCompositionLayerQuad){XR_TYPE_COMPOSITION_LAYER_QUAD};
    layer->layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    layer->space=head_space;layer->eyeVisibility=XR_EYE_VISIBILITY_BOTH;
    layer->pose.orientation.w=1;layer->pose.position=(XrVector3f){0.f,0.f,hud_menu?-1.8f:-1.5f};
    layer->size=(XrExtent2Df){hud_menu?1.65f:1.28f,hud_menu?.928125f:.72f};
    layer->subImage.swapchain=hud_chain;layer->subImage.imageRect.extent=(XrExtent2Di){CV_HUD_WIDTH,CV_HUD_HEIGHT};
    return 1;
}
static void hud_viewer_close(void){
    if(hud_chain)xrDestroySwapchain(hud_chain);
    glDeleteFramebuffers(1,&hud_framebuffer);
    munmap(game_hud,CV_HUD_MAP_BYTES);glDeleteTextures(1,&hud_texture);
}
