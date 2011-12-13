//
//  LaunchPlayMidiFilter.cpp
//  LaunchPlayVST
//
//  Created by Fred G on 07/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include "LaunchPlayMidiFilter.h"

using namespace LaunchPlayVST;

#pragma mark createEffectInstance
#if defined (LAUNCHPLAY_MIDIFILTER_EXPORT)
AudioEffect* createEffectInstance (audioMasterCallback audioMaster)
{
    return new LaunchPlayMidiFilter(audioMaster); 
}
#endif

#pragma mark LaunchPlayMidiFilter
LaunchPlayMidiFilter::LaunchPlayMidiFilter(audioMasterCallback audioMaster)
	: LaunchPlayBase(audioMaster, 0, 1), channelOffsetNumber_(0)
{ 
    setUniqueID(kEmUniqueID);
    noTail();
}

LaunchPlayMidiFilter::~LaunchPlayMidiFilter()
{

}

bool LaunchPlayMidiFilter::getEffectName(char* name)
{
	vst_strncpy (name, kMidiFilterName, kVstMaxEffectNameLen);
	return true;
}

VstInt32 LaunchPlayMidiFilter::canDo(char *text)
{
    if(strcmp(text, "sendVstMidiEvent") == 0 || 
       strcmp(text, "receiveVstMidiEvent") == 0 ||
       strcmp(text, "receiveVstTimeInfo") == 0)
        return 1;
    
	if(strcmp(text, "offline") == 0)
        return -1;
    
    return 0;
}    

float LaunchPlayMidiFilter::getParameter(VstInt32 index)
{
    switch (index) {
        case 0: // filter channel
            return normalizeValue(channelOffsetNumber_, kMaxMIDIChannelOffset);
    }
    
    return 0;
}

void LaunchPlayMidiFilter::setParameter(VstInt32 index, float value)
{
    switch (index) {
        case 0: // filter channel
			channelOffsetNumber_ = denormalizeValue(value, kMaxMIDIChannelOffset);
            break;
    }
}

void LaunchPlayMidiFilter::setParameterAutomated(VstInt32 index, float value)
{
    setParameter(index, value);
}

void LaunchPlayMidiFilter::getParameterDisplay(VstInt32 index, char *text)
{
    switch (index) {
        case 0: // filter channel
			std::stringstream ss;
			ss << (channelOffsetNumber_+1);
			vst_strncpy(text, ss.str().c_str(), kVstMaxParamStrLen);
			break;
    }
}

void LaunchPlayMidiFilter::getParameterName(VstInt32 index, char *text)
{
    switch (index) {
        case 0:
            vst_strncpy(text, "Channel", kVstMaxParamStrLen);
            break;
    }
}

VstInt32 LaunchPlayMidiFilter::processEvents(VstEvents *events) 
{
	assert(events != NULL);

 	VstEventsBlock::filterMidiEvents(events, channelOffsetNumber_);
	sendVstEventsToHost(events);

	return 0;
} 

void LaunchPlayMidiFilter::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{

}