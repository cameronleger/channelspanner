#ifndef CHANNELSPANNER_VST2_H
#define CHANNELSPANNER_VST2_H

#include "aeffect.h"
#include "aeffectx.h"

// Since the host is expecting a very specific API we need to make sure it has C linkage (not C++)
#ifdef __cplusplus
extern "C" {
#endif
extern AEffect* VSTPluginMain( audioMasterCallback vstHostCallback );
#ifdef __cplusplus
}
#endif

#endif //CHANNELSPANNER_VST2_H
