//
//  LaunchPlay.cpp
//  LaunchPlayVST
//
//  Created by Fred G on 03/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include "LaunchPlay.h"

AudioEffect* createEffectInstance (audioMasterCallback audioMaster)
{
	return new LaunchPlay(audioMaster);
}

LaunchPlay::LaunchPlay(audioMasterCallback audioMaster) : AudioEffectX (audioMaster, 0, 0)
{
    setNumInputs(2);
    setNumOutputs(2);
    
    setUniqueID(CCONST('f','9', '3', '5'));
    isSynth();
    noTail();
    canProcessReplacing();
}

LaunchPlay::~LaunchPlay()
{
    
}

void LaunchPlay::setParameter (VstInt32 index, float value)
{

}

float LaunchPlay::getParameter (VstInt32 index)
{
    return 0.f;
}

void LaunchPlay::getParameterName (VstInt32 index, char* label)
{

}

void LaunchPlay::getParameterDisplay (VstInt32 index, char* text)
{

}

void LaunchPlay::getParameterLabel (VstInt32 index, char* label)
{

}


bool LaunchPlay::getEffectName(char* name)
{
	vst_strncpy (name, "LaunchPlay", kVstMaxEffectNameLen);
	return true;
}

bool LaunchPlay::getProductString(char* text)
{
	vst_strncpy (text, "LaunchPlay", kVstMaxProductStrLen);
	return true;
}

bool LaunchPlay::getVendorString(char* text)
{
	vst_strncpy (text, "Fred G", kVstMaxVendorStrLen);
	return true;
}

VstInt32 LaunchPlay::getVendorVersion()
{ 
	return 1000; 
}

void LaunchPlay::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
    
}

VstInt32 LaunchPlay::processEvents(VstEvents *eventPtr) 
{
    for (VstInt32 i = 0; i < eventPtr->numEvents; ++i) {
        VstEvent *event = eventPtr->events[i];
        
        printf("Received event: type=%d byteSize=%d deltaFrames=%d data=%s\n", 
                event->type, event->byteSize, event->deltaFrames, event->data);
    }
     
    return 0;
}