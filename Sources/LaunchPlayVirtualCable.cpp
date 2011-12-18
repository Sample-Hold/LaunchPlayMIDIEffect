//
//  LaunchPlayVirtualCable.cpp
//  LaunchPlayVST
//
//  Created by Fred G on 07/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include "LaunchPlayVirtualCable.h"

using namespace LaunchPlayVST;
using namespace boost::interprocess;

#pragma mark createEffectInstance
AudioEffect* createEffectInstance (audioMasterCallback audioMaster)
{
    return new LaunchPlayVirtualCable(audioMaster); 
}

#pragma mark LaunchPlayVirtualCable
VstInt32 LaunchPlayVirtualCable::activeInstancesCount_ = 0;
VstInt32 LaunchPlayVirtualCable::maxMessageSize_ = 0;

LaunchPlayVirtualCable::LaunchPlayVirtualCable(audioMasterCallback audioMaster)
	: LaunchPlayBase(audioMaster, 0, 1), channelOffsetNumber_(0)
{ 
    setUniqueID(kVcUniqueID);
    noTail();
	programsAreChunks();
    
    ++activeInstancesCount_;
}

LaunchPlayVirtualCable::~LaunchPlayVirtualCable()
{
    --activeInstancesCount_;
}

bool LaunchPlayVirtualCable::getEffectName(char* name)
{
	vst_strncpy (name, kVirtualCableName, kVstMaxEffectNameLen);
	return true;
}

VstInt32 LaunchPlayVirtualCable::canDo(char *text)
{
    if(strcmp(text, "sendVstMidiEvent") == 0 || 
       strcmp(text, "sendVstEvents") == 0)
        return 1;
    
	if(strcmp(text, "offline") == 0)
        return -1;
    
    return 0;
}    

float LaunchPlayVirtualCable::getParameter(VstInt32 index)
{
    switch (index) {
        case 0: // virtual channel
            return normalizeValue(channelOffsetNumber_, kMaxMIDIChannelOffset);
    }
    
    return 0;
}

void LaunchPlayVirtualCable::setParameter(VstInt32 index, float value)
{
    switch (index) {
        case 0: // virtual channel
			channelOffsetNumber_ = denormalizeValue(value, kMaxMIDIChannelOffset);
            break;
    }
}

void LaunchPlayVirtualCable::getParameterDisplay(VstInt32 index, char *text)
{
    switch (index) {
        case 0: // virtual channel
			std::stringstream ss;
			ss << (channelOffsetNumber_+1);
			vst_strncpy(text, ss.str().c_str(), kVstMaxParamStrLen);
			break;
    }
}

void LaunchPlayVirtualCable::getParameterName(VstInt32 index, char *text)
{
    switch (index) {
        case 0:
            vst_strncpy(text, "Channel", kVstMaxParamStrLen);
            break;
    }
}

VstInt32 LaunchPlayVirtualCable::getChunk(void **data, bool isPreset) {
	*data = &channelOffsetNumber_;

	return sizeof(channelOffsetNumber_);
}

VstInt32 LaunchPlayVirtualCable::setChunk(void *data, VstInt32 byteSize, bool isPreset) {
	if(byteSize == sizeof(VstInt32)) {
		memcpy(&channelOffsetNumber_, data, byteSize);
	}

	return 0;
}

VstInt32 LaunchPlayVirtualCable::getNumMidiInputChannels()
{
	return 0;
}

VstInt32 LaunchPlayVirtualCable::getNumMidiOutputChannels()
{
	return 1;
}

void LaunchPlayVirtualCable::closeAllMessageQueues()
{
    for(VstInt32 i = 0; i <= kMaxMIDIChannelOffset; ++i) {
        std::stringstream ss;
        ss << kMessageQueueNames;
        ss << i;
        
        message_queue::remove(ss.str().c_str()); // never throws
    }
}

void LaunchPlayVirtualCable::open() 
{
	if(activeInstancesCount_ == 1)
		maxMessageSize_ = VstEventsBlock::getMaxSizeWhenSerialized();
}

void LaunchPlayVirtualCable::close() 
{
	if(activeInstancesCount_ == 1) // close all message queues
		closeAllMessageQueues();
}

void LaunchPlayVirtualCable::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
	try {
		boost::interprocess::message_queue::size_type message_size;
		unsigned int priority;
        
        std::stringstream ss;
        ss << kMessageQueueNames;
        ss << channelOffsetNumber_;
        
        char buffer[maxMessageSize_];
        
        permissions all_permissions;
        all_permissions.set_unrestricted();
        message_queue mq(open_or_create, ss.str().c_str(), kMaxQueueMessage, maxMessageSize_, all_permissions);

		if(mq.try_receive(buffer, maxMessageSize_, message_size, priority)) {
            std::stringbuf sb;   
            sb.pubsetbuf(buffer, message_size);

			// load serialized object
			VstEventsBlock eventsBlock;
			boost::archive::binary_iarchive archive(sb);
			archive >> eventsBlock;

			if(eventsBlock.numEvents > 0) {
				VstEvents *events = (VstEvents*) &eventsBlock;

				// change all midi channels to channel 1
				VstEventsBlock::forceMidiEventsChannelOffset(events, 0);

				sendVstEventsToHost(events);
			}
		}
	}
	catch(interprocess_exception) {
		printf("Exception occured with virtual cable %d\n", channelOffsetNumber_+1);
	}

	// null audio output
	for(VstInt32 i = 0; i < cEffect.numOutputs; ++i)
		memset(outputs[i], 0, sampleFrames*sizeof(float));
}