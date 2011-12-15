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
#include <boost/smart_ptr.hpp>
#include <boost/serialization/access.hpp>

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
    kMIDIPrimaryBufferCode	    = 0x34,
    kMIDISecondaryBufferCode	= 0x31,
    kMIDIVelocityMin            = 0,
    kMIDIVelocityAverage        = 0x40,
	kMIDIVelocityGood	        = 0x64,
    kMIDIVelocityMax            = 0x7F,
    kMIDIMiddleCNoteNumber      = 0x3C
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
        static void convertMidiEvent(VstMidiEvent *source, VstEvent *event);
		static void muteOtherMidiEvents(VstEvents *events, char channelOffset);
		static void forceMidiEventsChannelOffset(VstEvents *events, char channelOffset);
		VstEventsBlock getFilteredMidiEvents(char channelOffset);
        static void debugVstEvents(VstEvents const* events, char midiEventToWatch = 0);

		template<class Archive>
		void serialize(Archive & ar, const unsigned int version)
		{
			ar & numEvents;
			for(VstInt32 i = 0; i < numEvents; ++i) {
				ar & events[i];
			}
		}
    };

	typedef boost::shared_ptr<VstMidiEvent> VstMidiEventPtr;

	struct VstDelayedMidiEvent : public VstMidiEvent {
		double ppqStart;

		VstDelayedMidiEvent(VstMidiEventPtr const event, double ppq);
		VstMidiEventPtr toMidiEvent(VstInt32 frames);
	};

	typedef boost::shared_ptr<VstDelayedMidiEvent> VstDelayedMidiEventPtr;

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
			kScaleMaxValue = 11,
			kBaseNoteCount = 12,
			kBaseNoteMinOctave = -3,
			kBaseNoteMaxOctave = 3,
			kNoteOffsetMaxValue = 7,
			kMaxChannels = 16
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

        static VstMidiEventPtr createNoteOn(VstInt32 baseNoteOffset, 
										VstInt32 octave,
										Scale scale, 
										VstInt32 noteOffset, 
										VstInt32 deltaFrames,
										char channelOffset = 0);
        static VstMidiEventPtr createNoteOff(VstInt32 baseNoteOffset, 
										VstInt32 octave,
										Scale scale, 
										VstInt32 noteOffset, 
										VstInt32 deltaFrames,
										char channelOffset = 0);
    };
    
    struct LaunchPadUserInput {
        bool btnPressed, btnReleased, isGridButton, isTopButton, isRightButton;
        VstInt32 btnNumber, x, y;
    };
    
    class LaunchPadHelper {
        static VstMidiEventPtr createRawMessage(MIDIMessage const& message, 												
												VstInt32 deltaFrames,
												char flags = 0);
    public:
        enum LaunchPadLayout { 
			xYLayout = 1,
			drumRackLayout = 2 
		};

		enum LaunchPadFlags { 
			copyBothBuffers = 4,
			clearOtherBuffer = 8
		};

        enum LaunchPadLEDColor { 
			off = 0, 
            lowRed = 13, 
            fullRed = 15, 
            lowAmber = 29,
            fullAmber = 63, 
            fullYellow = 62, 
            lowGreen = 28, 
            fullGreen = 60 
		};

        // input messages
        static bool isValidMessage(MIDIMessage const& message);
        static LaunchPadUserInput readMessage(MIDIMessage const& message, 
                                              LaunchPadLayout layout);
        
        // output messages
        static VstMidiEventPtr createResetMessage(VstInt32 deltaFrames);

        static VstMidiEventPtr createSetLayoutMessage(LaunchPadLayout layout, 
                                                   VstInt32 deltaFrames);
        
        static VstMidiEventPtr createTopButtonMessage(VstInt32 number, 
                                                   LaunchPadLEDColor color,
                                                   VstInt32 deltaFrames, 
												   char flags = 0); 
        
        static VstMidiEventPtr createGridButtonMessage(size_t x, 
                                                    size_t y,
                                                    LaunchPadLEDColor color,
                                                    VstInt32 deltaFrames,
													char flags = 0);

		static VstMidiEventPtr createRightButtonMessage(VstInt32 number,
                                                    LaunchPadLEDColor color,
                                                    VstInt32 deltaFrames,
													char flags = 0);

        static VstMidiEventPtr createSwapBuffersMessage(bool primaryBuffer, 
                                                     VstInt32 deltaFrames);
    };
    
} // } namespace LaunchPlayVST

namespace boost {
namespace serialization {

	template<class Archive>
	void serialize(Archive & ar, VstEvent &event, const unsigned int version)
	{
		ar & event.type;
		ar & event.byteSize;
		ar & event.deltaFrames;
		ar & event.flags;
		ar & event.data;
	}

} // namespace serialization
} // namespace boost

#endif