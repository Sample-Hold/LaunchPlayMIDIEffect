//
//  LaunchPlayEmitter.cpp
//  LaunchPlayVST
//
//  Created by Fred G on 07/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include "LaunchPlayEmitter.h"

using namespace LaunchPlayVST;
using namespace boost::interprocess;

#pragma mark createEffectInstance
#if defined (LAUNCHPLAY_EMITTER_EXPORT)
AudioEffect* createEffectInstance (audioMasterCallback audioMaster)
{
    return new LaunchPlayEmitter(audioMaster); 
}
#endif

#pragma mark LaunchPlayEmitter
LaunchPlayEmitter::LaunchPlayEmitter(audioMasterCallback audioMaster)
: LaunchPlayBase(audioMaster, 0, 0)
{ 
    setUniqueID(kEmUniqueID);
    noTail();
}

LaunchPlayEmitter::~LaunchPlayEmitter()
{
    
}

bool LaunchPlayEmitter::getEffectName(char* name)
{
	vst_strncpy (name, kEmitterName, kVstMaxEffectNameLen);
	return true;
}

VstInt32 LaunchPlayEmitter::canDo(char *text)
{
    if(strcmp(text, "sendVstMidiEvent") == 0)
        return 1;
    
	if(strcmp(text, "offline") == 0)
        return -1;
    
    return 0;
}    

void LaunchPlayEmitter::open()
{
    mq_.reset(new message_queue(open_only, kMessageQueueName));
}

void LaunchPlayEmitter::close()
{
    mq_.release();
}

void LaunchPlayEmitter::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
    unsigned int priority;
    message_queue::size_type recvd_size;
    
    VstEventsBlock eventsBlock;
    eventsBlock.allocate(kVstEventsBlockSize);

    if(mq_.get() != NULL && mq_->try_receive(&eventsBlock, sizeof(eventsBlock), recvd_size, priority)) {
        VstEvents *events = (VstEvents*) &eventsBlock;
        sendVstEventsToHost(events);
    }

    eventsBlock.deallocate();
}
