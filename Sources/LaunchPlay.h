//
//  LaunchPlay.h
//  LaunchPlayVST
//
//  Created by Fred G on 05/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef LaunchPlayVST_LaunchPlay_h
#define LaunchPlayVST_LaunchPlay_h

#if defined (WIN32)
	#define _CRT_SECURE_NO_WARNINGS 1
	#pragma warning (disable : 4068 ) /* disable unknown pragma warnings */
#endif

#include <assert.h>
#include <stdio.h>
#include <sstream>
#include <audioeffectx.h>

#define kProductString      "LaunchPlay sequencer for LaunchPad"
#define kVendorString       "Fred G for sample-hold.com"
#define kSequencerName      "LaunchPlaySequencer"
#define kMidiFilterName     "LaunchPlayMidiFilter"
#define kVendorVersion      1000
#define kSeqUniqueID        CCONST('f','9', 's', 'q')
#define kEmUniqueID         CCONST('f','9', 'e', 'm')

namespace LaunchPlayVST {
    
	class LaunchPlayBase : public AudioEffectX {
	protected:
		static VstInt32 denormalizeValue(float const value, VstInt32 const max);
		static float normalizeValue(VstInt32 const value, VstInt32 const max);
    public:
        LaunchPlayBase(audioMasterCallback audioMaster, 
                       VstInt32 numPrograms, 
                       VstInt32 numParams);
		virtual ~LaunchPlayBase();
        
        bool getVendorString(char *text);
        bool getProductString(char *text);
        VstInt32 getVendorVersion();
    };
    
} // } namespace LaunchPlayVST

#endif