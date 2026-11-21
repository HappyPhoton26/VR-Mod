/* Read-only OpenXR resolution query: creates no session or swapchain. */
#include <openxr/openxr.h>
#include <stdio.h>
#include <string.h>
int main(void){
 XrInstanceCreateInfo ci={.type=XR_TYPE_INSTANCE_CREATE_INFO};
 strcpy(ci.applicationInfo.applicationName,"CoD2 VR resolution query");ci.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
 XrInstance instance;XrResult r=xrCreateInstance(&ci,&instance);if(XR_FAILED(r)){fprintf(stderr,"xrCreateInstance %d\n",r);return 1;}
 XrSystemGetInfo gi={.type=XR_TYPE_SYSTEM_GET_INFO,.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};
 XrSystemId system;r=xrGetSystem(instance,&gi,&system);if(XR_FAILED(r)){fprintf(stderr,"xrGetSystem %d\n",r);xrDestroyInstance(instance);return 2;}
 XrSystemProperties properties={.type=XR_TYPE_SYSTEM_PROPERTIES};r=xrGetSystemProperties(instance,system,&properties);
 if(XR_SUCCEEDED(r))printf("system=%s max_swapchain=%ux%u max_layers=%u\n",properties.systemName,properties.graphicsProperties.maxSwapchainImageWidth,properties.graphicsProperties.maxSwapchainImageHeight,properties.graphicsProperties.maxLayerCount);
 XrViewConfigurationView views[2]={{.type=XR_TYPE_VIEW_CONFIGURATION_VIEW},{.type=XR_TYPE_VIEW_CONFIGURATION_VIEW}};uint32_t count=0;
 r=xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&count,views);
 if(XR_SUCCEEDED(r))for(uint32_t i=0;i<count&&i<2;i++)printf("eye=%u recommended=%ux%u maximum=%ux%u samples_recommended=%u samples_max=%u\n",i,views[i].recommendedImageRectWidth,views[i].recommendedImageRectHeight,views[i].maxImageRectWidth,views[i].maxImageRectHeight,views[i].recommendedSwapchainSampleCount,views[i].maxSwapchainSampleCount);
 xrDestroyInstance(instance);return XR_FAILED(r)?3:0;
}
