//
//  MIDIHelper.h
//  LaunchPlayVST
//
//  Created by Fred G on 08/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef LaunchPlayVST_MIDIHelper_h
#define LaunchPlayVST_MIDIHelper_h

#include <aeffect.h>

#define SIZEOFMIDIEVENT         sizeof(VstMidiEvent) - 2 * sizeof(VstInt32)
#define kVstEventsBlockSize     32

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
    kMIDIVelocityMax            = 127,
    kMIDIMiddleCNoteNumber      = 60
};

namespace LaunchPlayVST {
    
    struct MIDIMessage {
        unsigned char code, arg1, arg2;
        MIDIMessage(unsigned char c, unsigned char a1, unsigned char a2);
        MIDIMessage(VstMidiEvent const* event);
    };

    struct VstEventsBlock {
        VstInt32 numEvents;
        VstIntPtr reserved;
        VstEvent* events[kVstEventsBlockSize]; // crap; thanks a lot Steinberg for this
        
        void allocate(size_t const size);
        void deallocate();
        static void convertMidiEvent(VstMidiEvent const& midiEvent, VstEvent *event);
        static void debugVstEvents(VstEvents const* events, char midiEventToWatch = 0);
    };
    
    class MIDIHelper {
        static const int majorScaleOffsets[];
        static const int minorScaleOffsets[];
    public:
        enum Scale { majorScale, minorScale };
        
        enum {
            kBaseNoteMaxValue = 11,
            kScaleMaxValue = 1,
        };
        
        static VstMidiEvent createNoteOn(VstInt32 baseNoteOffset, 
                                       Scale scale, 
                                       VstInt32 offset, 
                                       VstInt32 deltaFrames);
        static VstMidiEvent createNoteOff(VstInt32 baseNoteOffset, 
                                          Scale scale, 
                                          VstInt32 offset, 
                                          VstInt32 deltaFrames);
    };
    
    struct LaunchPadUserInput {
        bool btnPressed, btnReleased, isGridButton, isTopButton, isRightButton;
        VstInt32 btnNumber, x, y;
    };
    
    class LaunchPadHelper {
    protected:
        static VstMidiEvent createRawMessage(MIDIMessage const& message, 
                                             bool copyBit,
                                             bool clearBit, 
                                             VstInt32 deltaFrames);
    public:
        enum LaunchPadLayout { xYLayout = 1, drumRackLayout = 2 };
        
        enum LaunchPadLEDColor { off = 0, 
            lightRed = 1, 
            fullRed = 3, 
            lowAmber = 17,
            fullAmber = 51, 
            fullYellow = 50, 
            lowGreen = 16, 
            fullGreen = 48 };
        
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
        
        static VstMidiEvent createSwapBuffersMessage(bool primaryBuffer, 
                                                     VstInt32 deltaFrames);
    };
    
} // } namespace LaunchPlayVST

#endif