//
//  LaunchPlay.h
//  LaunchPlayVST
//
//  Created by Fred G on 03/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef LaunchPlayVST_LaunchPlay_h
#define LaunchPlayVST_LaunchPlay_h

class LaunchPlay : public AudioEffectX
{
public:
	LaunchPlay(audioMasterCallback audioMaster);
	virtual ~LaunchPlay();
    
    // Category
    virtual VstPlugCategory getPlugCategory() { return kPlugCategSynth; }
    
    // Vendor infos
    virtual bool getEffectName(char* name);
	virtual bool getVendorString(char* text);
	virtual bool getProductString(char* text);
	virtual VstInt32 getVendorVersion();
    
	// Parameters
	virtual void setParameter (VstInt32 index, float value);
	virtual float getParameter (VstInt32 index);
	virtual void getParameterLabel (VstInt32 index, char* label);
	virtual void getParameterDisplay (VstInt32 index, char* text);
	virtual void getParameterName (VstInt32 index, char* text);
    
    // Handle events
    virtual VstInt32 processEvents(VstEvents *eventPtr);

    virtual void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames);
};

#endif
