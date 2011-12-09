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

#define SIZEOFMIDIEVENT             sizeof(VstMidiEvent) - 2 * sizeof(VstInt32)

#define kMIDILBitMask               0xF0
#define kMIDIRBitMask               0x0F
#define kMIDINoteOffEvent           0x80
#define kMIDINoteOnEvent            0x90
#define kMIDIControllerChangeEvent  0xB0
#define kMIDIPrimaryBufferAdress    0x31
#define kMIDISecondaryBufferAdress  0x34
#define kMIDIVelocityMin            64
#define kMIDIVelocityMax            127
#define kMIDIMiddleCNoteNumber      60 

namespace LaunchPlayVST {
    
    struct MIDIMessage {
        unsigned char code, arg1, arg2;
        MIDIMessage(unsigned char c, unsigned char a1, unsigned char a2);
        MIDIMessage(VstMidiEvent const* event);
    };
    
    struct VstEventsArray {
        VstInt32 numEvents;
        VstIntPtr reserved;
        VstEvent **events;
    };
    
    struct VstMIDIHelper {
        static VstEventsArray* allocateVstEventsArray(size_t numEvents);
        static void freeVstEventsArray(VstEventsArray* array);
    };
    
    enum LaunchPadLayout { xYLayout = 1, drumRackLayout = 2 };
    enum LaunchPadLEDColor { off = 12, lightRed = 13, fullRed = 15, lowAmber = 29,
        fullAmber = 63, fullYellow = 62, lowGreen = 28, fullGreen = 60, 
        flashingRed = 11, flashingAmber = 59, flashingYellow = 58, flashingGreen = 56 };
    
    struct LaunchPadUserInput {
        bool btnPressed, btnReleased, isGridButton, isTopButton, isRightButton;
        VstInt32 btnNumber, x, y;
    };
    
    class LaunchPadMessagesHelper {
    protected:
        static VstMidiEvent* createRawMessage(MIDIMessage const& message, 
                                              VstInt32 deltaFrames);
    public:
        // input messages
        static bool isValidMessage(VstMidiEvent const* event);
        static LaunchPadUserInput readMessage(VstMidiEvent const* event, 
                                              LaunchPadLayout layout);
        
        // output messages
        static VstMidiEvent* createResetMessage(VstInt32 deltaFrames = 0);
        static VstMidiEvent* createSetLayoutMessage(LaunchPadLayout layout, 
                                                    VstInt32 deltaFrames = 0);
        static VstMidiEvent* createChangeDirectionButtonMessage(VstInt32 direction, 
                                                                LaunchPadLEDColor color,
                                                                VstInt32 deltaFrames = 0); 
        static VstMidiEvent* createChangeGridButtonMessage(VstInt32 x, 
                                                           VstInt32 y,
                                                           LaunchPadLEDColor color,
                                                           VstInt32 deltaFrames = 0);
        static VstMidiEvent* createSwapBuffersMessage(bool primaryBuffer, 
                                                      VstInt32 deltaFrames = 0);
    };
    
} // } namespace LaunchPlayVST

#endif
