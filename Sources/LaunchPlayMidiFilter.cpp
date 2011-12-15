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
AudioEffect* createEffectInstance (audioMasterCallback audioMaster)
{
    return new LaunchPlayMidiFilter(audioMaster); 
}

#pragma mark LaunchPlayMidiFilter
LaunchPlayMidiFilter::LaunchPlayMidiFilter(audioMasterCallback audioMaster)
	: LaunchPlayBase(audioMaster, 0, 1), channelOffsetNumber_(0)
{ 
    setUniqueID(kEmUniqueID);
    noTail();
	programsAreChunks();
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
	   strcmp(text, "receiveVstMidiEvent") == 0)
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

VstInt32 LaunchPlayMidiFilter::getChunk(void **data, bool isPreset) {
	*data = &channelOffsetNumber_;

	return sizeof(channelOffsetNumber_);
}

VstInt32 LaunchPlayMidiFilter::setChunk(void *data, VstInt32 byteSize, bool isPreset) {
	if(byteSize == sizeof(VstInt32)) {
		memcpy(&channelOffsetNumber_, data, byteSize);
	}

	return 0;
}

VstInt32 LaunchPlayMidiFilter::processEvents(VstEvents *events) 
{
	assert(events != NULL);

 	VstEventsBlock::muteOtherMidiEvents(events, channelOffsetNumber_);
	sendVstEventsToHost(events);

	return 0;
} 

VstInt32 LaunchPlayMidiFilter::getNumMidiInputChannels()
{
	return 1;
}

VstInt32 LaunchPlayMidiFilter::getNumMidiOutputChannels()
{
	return kMaxMIDIChannelOffset + 1;
}

void LaunchPlayMidiFilter::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
	// null audio output
	for(VstInt32 i = 0; i < cEffect.numOutputs; ++i)
		memset(outputs[i], 0, sampleFrames*sizeof(float));
}