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
        void getParameterName(VstInt32 index, char *text);
        void getParameterDisplay(VstInt32 index, char *text);
        bool canParameterBeAutomated (VstInt32 index) { return false; } 

		VstInt32 getNumMidiInputChannels();
		VstInt32 getNumMidiOutputChannels();

		VstInt32 getChunk(void **data, bool isPreset = false);	
		VstInt32 setChunk(void *data, VstInt32 byteSize, bool isPreset=false);

		VstInt32 processEvents(VstEvents *events);
        void processReplacing (float **inputs, float **outputs, VstInt32 sampleFrames);
    };
    
} // } namespace LaunchPlayVST

#endif
