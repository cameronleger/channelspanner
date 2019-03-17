#ifndef CHANNELSPANNER_VST2_H
#define CHANNELSPANNER_VST2_H

#ifdef VESTIGE

#include <stdint.h>
typedef int16_t VstInt16;
typedef int32_t VstInt32;
typedef int64_t VstInt64;
typedef VstInt64 VstIntPtr;
#define __cdecl
#define VESTIGECALLBACK __cdecl
#include "vestige.h"

#define kVstMaxParamStrLen 8
#define kVstMaxProductStrLen kVstMaxVendorStrLen
#define kPlugCategAnalysis 3

#else
#include "aeffect.h"
#include "aeffectx.h"
#endif

// Since the host is expecting a very specific API we need to make sure it has C linkage (not C++)
#ifdef __cplusplus
extern "C" {
#endif
extern AEffect* VSTPluginMain( audioMasterCallback vstHostCallback );
#ifdef __cplusplus
}
#endif

#endif //CHANNELSPANNER_VST2_H
