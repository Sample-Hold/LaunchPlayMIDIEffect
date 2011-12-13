//
//  LaunchPlayMidiFilter.h
//  LaunchPlayVST
//
//  Created by Fred G on 07/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef LaunchPlayVST_LaunchPlayMidiFilter_h
#define LaunchPlayVST_LaunchPlayMidiFilter_h

#include "LaunchPlay.h"
#include "MIDIHelpers.h"

#define kMaxMIDIChannelOffset 8

namespace LaunchPlayVST {
    
    class LaunchPlayMidiFilter : public LaunchPlayBase {
		VstInt32 channelOffsetNumber_;
    public:
        LaunchPlayMidiFilter(audioMasterCallback audioMaster);
        ~LaunchPlayMidiFilter();
        
        VstPlugCategory getPlugCategory() { return kPlugCategEffect; }
        bool getEffectName(char *name);
        VstInt32 canDo(char *text);

		float getParameter(VstInt32 index);
        void setParameter(VstInt32 index, float value);
        void setParameterAutomated(VstInt32 index, float value);
        void getParameterName(VstInt32 index, char *text);
        void getParameterDisplay(VstInt32 index, char *text);

		VstInt32 processEvents(VstEvents *events);
        void processReplacing (float **inputs, float **outputs, VstInt32 sampleFrames);
    };
    
} // } namespace LaunchPlayVST

#endif
