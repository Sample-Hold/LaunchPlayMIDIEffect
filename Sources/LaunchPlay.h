//
//  LaunchPlay.h
//  LaunchPlayVST
//
//  Created by Fred G on 03/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef LaunchPlayVST_LaunchPlay_h
#define LaunchPlayVST_LaunchPlay_h

#include <audioeffectx.h>

#define kNumPrograms        0
#define kNumParameters      0
#define kMaxAudioInputs     2
#define kMaxAudioOutputs    2
#define kDefaultTempo       120
#define kMIDIMessageSize    24

namespace LaunchPlayVST {

class LaunchPlay : public AudioEffectX
{
    VstTimeInfo currentTimeInfo_;
    double beatsPerSample_;
protected:    
    void initVST(audioMasterCallback audioMaster);
    void initTimeInfo();
public:
	LaunchPlay(audioMasterCallback audioMaster);
	virtual ~LaunchPlay();
    
    // Category
    virtual VstPlugCategory getPlugCategory() { return kPlugCategSynth; }
    
    // Vendor infos
    virtual bool getEffectName(char *name);
	virtual bool getVendorString(char *text);
	virtual bool getProductString(char *text);
	virtual VstInt32 getVendorVersion();
    virtual VstInt32 canDo(char *text);
    
    // Handle MIDI events
    virtual VstInt32 processEvents(VstEvents *eventPtr);

    // Main process
    virtual void processReplacing (float **inputs, float **outputs, VstInt32 sampleFrames);
};
    
} // } namespace LaunchPlayVST

#endif