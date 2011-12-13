//
//  MIDIHelper.h
//  LaunchPlayVST
//
//  Created by Fred G on 08/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef LaunchPlayVST_MIDIHelper_h
#define LaunchPlayVST_MIDIHelper_h

#include <assert.h>
#include <stdio.h>
#include <pluginterfaces/vst2.x/aeffectx.h>

#if defined (WIN32)
	#define _CRT_SECURE_NO_WARNINGS 1
	#pragma warning (disable : 4068 ) /* disable unknown pragma warnings */
#endif

#define SIZEOFMIDIEVENT         sizeof(VstMidiEvent) - 2 * sizeof(VstInt32)

enum {
    kMIDILBitMask               = 0xF0,
    kMIDIRBitMask               = 0x0F,
    kMIDILRBitMask              = 0xFF,
    kMIDINoteOffEvent           = 0x80,
    kMIDINoteOnEvent            = 0x90,
    kMIDIControllerChangeEvent  = 0xB0,
    kMIDIPrimaryBufferAdress    = 0x31,
    kMIDISecondaryBufferAdress  = 0x34,
    kMIDIVelocityMin            = 0,
    kMIDIVelocityAverage        = 64,
	kMIDIVelocityGood	        = 100,
    kMIDIVelocityMax            = 127,
    kMIDIMiddleCNoteNumber      = 60
};

namespace LaunchPlayVST {
    
    struct MIDIMessage {
        char code, arg1, arg2;
        MIDIMessage(char c, char a1, char a2);
        MIDIMessage(VstMidiEvent const* event);
    };

    struct VstEventsBlock {
		enum { kVstEventsBlockSize = 200 };

        VstInt32 numEvents;
        VstIntPtr reserved;
        VstEvent* events[kVstEventsBlockSize]; // crap, thanks Steinberg for this
        
        void allocate(size_t const size);
        void deallocate();
        static void convertMidiEvent(VstMidiEvent const& midiEvent, VstEvent *event);
		static void filterMidiEvents(VstEvents *events, char channelOffset);
        static void debugVstEvents(VstEvents const* events, char midiEventToWatch = 0);
    };

	struct VstDelayedMidiEvent : public VstMidiEvent {
		double ppqStart;

		VstDelayedMidiEvent(VstMidiEvent const& event, double ppq);
		VstMidiEvent toMidiEvent(VstInt32 deltaFrames);
	};

    class MIDIHelper {
        static const int logScaleMajorOffsets[];
        static const int logScaleMinorNatOffsets[];
		static const int logScaleMinorHarOffsets[];
		static const int logScaleMinorMelAscOffsets[];
		static const int logScaleMinorMelDescOffsets[];
		static const int pentScaleMajorOffsets[];
		static const int pentScaleMinorNatOffsets[];
		static const int hexScaleWholeOffsets[];
		static const int hexScaleAugOffsets[];
		static const int hexScalePromOffsets[];
		static const int hexScaleBluesOffsets[];
		static const int hexScaleTritoneOffsets[];
    public:
		enum {
            kBaseNoteOffsetMaxValue = 11,
			kBaseNoteCount = 12,
			kBaseNoteMinOctave = -4,
			kBaseNoteMaxOctave = 3,
			kNoteOffsetMaxValue = 7,
			kChannelOffsetMaxValue = 15,
			kMaxChannels = 16,
            kScaleMaxValue = 11
        };

        enum Scale { 
			logScaleMajor, 
			logScaleMinorNat,
			logScaleMinorHar, 
			logScaleMinorMelAsc,
			logScaleMinorMelDesc,
			pentScaleMajor,
			pentScaleMinorNat,
			hexScaleWhole,
			hexScaleAug,
			hexScaleProm,
			hexScaleBlues,
			hexScaleTritone
		};

        static VstMidiEvent createNoteOn(VstInt32 baseNoteOffset, 
										VstInt32 octave,
                                       Scale scale, 
                                       VstInt32 noteOffset,
									   VstInt32 channelOffset,
                                       VstInt32 deltaFrames);
        static VstMidiEvent createNoteOff(VstInt32 baseNoteOffset, 
										  VstInt32 octave,
                                          Scale scale, 
                                          VstInt32 noteOffset, 
										  VstInt32 channelOffset,
                                          VstInt32 deltaFrames);
    };
    
    struct LaunchPadUserInput {
        bool btnPressed, btnReleased, isGridButton, isTopButton, isRightButton;
        VstInt32 btnNumber, x, y;
    };
    
    class LaunchPadHelper {
        static VstMidiEvent createRawMessage(MIDIMessage const& message, 
                                             bool copyBit,
                                             bool clearBit, 
                                             VstInt32 deltaFrames);
    public:
        enum LaunchPadLayout { 
			xYLayout = 1,
			drumRackLayout = 2 
		};
        
        enum LaunchPadLEDColor { 
			off = 0, 
            lightRed = 1, 
            fullRed = 3, 
            lowAmber = 17,
            fullAmber = 51, 
            fullYellow = 50, 
            lowGreen = 16, 
            fullGreen = 48 
		};

        // input messages
        static bool isValidMessage(VstMidiEvent const* event);
        static LaunchPadUserInput readMessage(VstMidiEvent const* event, 
                                              LaunchPadLayout layout);
        
        // output messages
        static VstMidiEvent createResetMessage(VstInt32 deltaFrames);

        static VstMidiEvent createSetLayoutMessage(LaunchPadLayout layout, 
                                                   VstInt32 deltaFrames);
        
        static VstMidiEvent createTopButtonMessage(VstInt32 number, 
                                                   LaunchPadLEDColor color,
                                                   VstInt32 deltaFrames); 
        
        static VstMidiEvent createGridButtonMessage(size_t x, 
                                                    size_t y,
                                                    LaunchPadLEDColor color,
                                                    VstInt32 deltaFrames);

		static VstMidiEvent createRightButtonMessage(VstInt32 number,
                                                    LaunchPadLEDColor color,
                                                    VstInt32 deltaFrames);

        static VstMidiEvent createSwapBuffersMessage(bool primaryBuffer, 
                                                     VstInt32 deltaFrames);
    };
    
} // } namespace LaunchPlayVST

#endif