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
#include <string.h>
#include <sstream>
#include <audioeffectx.h>

#define kProductString          "LaunchPlay sequencer for LaunchPad"
#define kVendorString           "Fred G for sample-hold.com"
#define kSequencerName          "LaunchPlaySequencer"
#define kMidiFilterName         "LaunchPlayMidiFilter"
#define kVirtualCableName       "LaunchPlayVirtualCable"
#define kVendorVersion          1000
#define kSeqUniqueID            CCONST('f','9', 's', 'q')
#define kEmUniqueID             CCONST('f','9', 'e', 'm')
#define kVcUniqueID             CCONST('f','9', 'v', 'c')

#define kMessageQueueNames      "LaunchPlayVirtualCable_chan"
#define kMaxQueueMessage		32
#define kMaxMIDIChannelOffset	8
#define kDefaultTempo			120

#define kStrideHalf				2
#define kStrideQuarter			1
#define kStrideEight			.5
#define kStrideSixteenth		.25
#define kStrideThirtysecond		.125

namespace LaunchPlayVST {
    
	class LaunchPlayBase : public AudioEffectX {
		double currentTempo_, currentBeatsPerSample_;
	protected:
		static VstInt32 denormalizeValue(float const value, VstInt32 const max);
		static float normalizeValue(VstInt32 const value, VstInt32 const max);

		void detectTicks(VstTimeInfo *timeInfo, 
                         VstInt32 sampleFrames, 
                         double stride);
		virtual void onTick(double tempo, double ppq, double sampleRate, VstInt32 sampleOffset) = 0; 
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