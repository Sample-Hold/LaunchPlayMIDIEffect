//
//  LaunchPlay.cpp
//  LaunchPlayVST
//
//  Created by Fred G on 03/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include "LaunchPlay.h"
#include "LaunchPlayVersion.h"

#include <vector>
#include <math.h>

#define BEATSPERSAMPLE(sampleRate, tempo) 1.0 / (sampleRate) / 60.0 * (tempo)

using namespace LaunchPlayVST;

AudioEffect* createEffectInstance (audioMasterCallback audioMaster)
{
	return new LaunchPlay(audioMaster);
}

LaunchPlay::LaunchPlay(audioMasterCallback audioMaster): 
        AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{ 
    initVST(audioMaster);
    initTimeInfo();
}

void LaunchPlay::initVST(audioMasterCallback audioMaster) 
{
    setNumInputs(kMaxAudioInputs);
    setNumOutputs(kMaxAudioOutputs);
    
    setUniqueID(kUniqueID);
    noTail();
    canProcessReplacing();
}

void LaunchPlay::initTimeInfo() 
{
    currentTimeInfo_.ppqPos = (currentTimeInfo_.samplePos = 0);
    currentTimeInfo_.sampleRate = sampleRate;
    currentTimeInfo_.tempo = kDefaultTempo;
    beatsPerSample_ = BEATSPERSAMPLE(currentTimeInfo_.sampleRate, currentTimeInfo_.tempo);
}

LaunchPlay::~LaunchPlay()
{

}

bool LaunchPlay::getEffectName(char* name)
{
	vst_strncpy (name, kEffectName, kVstMaxEffectNameLen);
	return true;
}

bool LaunchPlay::getProductString(char* text)
{
	vst_strncpy (text, kProductString, kVstMaxProductStrLen);
	return true;
}

bool LaunchPlay::getVendorString(char* text)
{
	vst_strncpy (text, kVendorString, kVstMaxVendorStrLen);
	return true;
}

VstInt32 LaunchPlay::getVendorVersion()
{ 
	return kVendorVersion; 
}

void LaunchPlay::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
    VstTimeInfo *time = getTimeInfo(kVstTransportChanged | kVstTransportPlaying | kVstTempoValid | kVstPpqPosValid); 

    if (time 
        && time->flags & kVstTransportChanged
        && time->flags & kVstTempoValid
        && time->flags & kVstPpqPosValid)
    {
        currentTimeInfo_.ppqPos = time->ppqPos;
        currentTimeInfo_.tempo = time->tempo;
        currentTimeInfo_.sampleRate = time->sampleRate;
        currentTimeInfo_.samplePos = time->samplePos;
        beatsPerSample_ = BEATSPERSAMPLE(currentTimeInfo_.sampleRate, currentTimeInfo_.tempo);
    }
    
    if (time && time->flags & kVstTransportPlaying) {
        
        double cycleStartPos = currentTimeInfo_.ppqPos;
        double cycleEndPos = cycleStartPos + sampleFrames * beatsPerSample_;
        
        // find first 1/8th tick
        double currentPos = ceil(cycleStartPos) - .5;
        if(currentPos < cycleStartPos)
            currentPos = fabs(ceil(currentPos));
        
        for(;currentPos < cycleEndPos; currentPos += .5){
            
            VstEvent midiTickOn, midiTickOff;
            midiTickOn.type = (midiTickOff.type = kVstMidiType);
            midiTickOn.byteSize = (midiTickOff.byteSize = kMIDIMessageSize);
            
            midiTickOn.deltaFrames = VstInt32((currentPos - cycleStartPos) / beatsPerSample_);
            midiTickOn.data[0] = 0x80; // note on
            midiTickOn.data[1] = 64; // note number
            midiTickOn.data[2] = 100; // velocity
			midiTickOn.data[3] = 0; // stop
            
            midiTickOff.deltaFrames = VstInt32(midiTickOn.deltaFrames + .25 / beatsPerSample_);
            midiTickOff.data[0] = 0x90; // note off
            midiTickOff.data[1] = 64; // note number
            midiTickOff.data[2] = 64; // velocity                    
			midiTickOff.data[3] = 0; // stop

            VstEvents tickEvents;
            tickEvents.numEvents = 2;
            tickEvents.events[0] = &midiTickOn;
            tickEvents.events[1] = &midiTickOff;
            
            printf("sending MIDI tick at pos %1.3f\n ", currentPos);
            sendVstEventsToHost(&tickEvents);
        }
        
        // save end ppq for next block
        currentTimeInfo_.ppqPos = cycleEndPos;
    }  
}

VstInt32 LaunchPlay::canDo(char *text)
{
    if(strcmp(text, "sendVstMidiEvent") == 0 || 
       strcmp(text, "receiveVstMidiEvent") == 0 ||
       strcmp(text, "receiveVstTimeInfo") == 0)
        return 1;

	if(strcmp(text, "offline") == 0)
        return -1;

    return AudioEffectX::canDo(text);
}                     

VstInt32 LaunchPlay::processEvents(VstEvents *eventPtr) 
{
    for (VstInt32 i = 0; i < eventPtr->numEvents; ++i) {
        VstEvent *event = eventPtr->events[i]; 
        
        if(event->type != kVstMidiType)
            continue;
        
        VstMidiEvent *midiEvent = (VstMidiEvent*) event;
        unsigned char midiCode = midiEvent->midiData[0]&0xF0;
        
        if(midiCode == 0xB0) // CC
            printf("received CC: code=%X id=%X on|off=%d\n",
               midiEvent->midiData[0]&0xFF,  midiEvent->midiData[1],  midiEvent->midiData[2]);
        else if(midiCode == 0x80 || midiCode == 0x90) // note on-off
            printf("received note: code=%X number=%d velocity=%d\n",
                   midiEvent->midiData[0]&0xFF,  midiEvent->midiData[1],  midiEvent->midiData[2]);
    }
     
    return 0;
}