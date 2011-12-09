//
//  MIDIHelper.cpp
//  LaunchPlayVST
//
//  Created by Fred G on 08/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include "MIDIHelper.h"

using namespace LaunchPlayVST;

#pragma mark MIDIMessage
MIDIMessage::MIDIMessage(VstMidiEvent const* event)
{
    assert(event != NULL);
    
    code = event ? event->midiData[0] : 0;
    arg1 = event ? event->midiData[1] : 0;
    arg2 = event ? event->midiData[2] : 0;
}

MIDIMessage::MIDIMessage(unsigned char c, unsigned char a1, unsigned char a2)
: code(c), arg1(a1), arg2(a2)
{
    
}

#pragma mark VstMIDIHelper
VstEventsArray* VstMIDIHelper::allocateVstEventsArray(size_t numEvents)
{
    VstEventsArray* newEventsArray = new VstEventsArray;
    newEventsArray->numEvents = VstInt32(numEvents);
    newEventsArray->events = new VstEvent*; 
    
    return newEventsArray;
}

void VstMIDIHelper::freeVstEventsArray(VstEventsArray* array)
{
    assert(array != NULL && array->events != NULL);
    
    for (size_t i = 0; i < array->numEvents; ++i)
        if(array->events[i] != NULL)
            delete array->events[i];
    
    delete array->events;
    delete array;
}

#pragma mark LaunchPadMessagesHelper
bool LaunchPadMessagesHelper::isValidMessage(VstMidiEvent const* event)
{
    assert(event->type == kVstMidiType);
    
    MIDIMessage message(event);
    
    return (message.code & kMIDILBitMask) == kMIDINoteOnEvent ||
           (message.code & kMIDILBitMask) == kMIDINoteOffEvent ||
           (message.code & kMIDILBitMask) == kMIDIControllerChangeEvent;
}

LaunchPadUserInput LaunchPadMessagesHelper::readMessage(VstMidiEvent const* event, 
                                                        LaunchPadLayout layout)
{
    assert(event->type == kVstMidiType);
    
    MIDIMessage message(event);
    LaunchPadUserInput userInput;
    
    userInput.btnPressed = (message.arg2 == kMIDIVelocityMax);
    userInput.btnReleased = (message.arg2 == kMIDIVelocityMin);
    userInput.isTopButton = (message.code == kMIDIControllerChangeEvent);
    
    if (userInput.isTopButton) {
        userInput.isGridButton = (userInput.isRightButton = false);
        userInput.x = (userInput.y = -1);
        userInput.btnNumber = message.arg1 - 104;
    }
    else if(layout == xYLayout) {
        userInput.isGridButton = !(userInput.isRightButton = 
                                   (message.arg1 & kMIDILBitMask) >> 4 < 8 &&
                                   (message.arg1 & kMIDIRBitMask) == 8);
        if(userInput.isGridButton) {
            userInput.x = (message.arg1 & kMIDIRBitMask);
            userInput.y = (message.arg1 & kMIDILBitMask) >> 4;
            userInput.btnNumber = userInput.y * 8 + userInput.x;
        }
        else { // right button
            userInput.x = (userInput.y = -1);
            userInput.btnNumber = (message.arg1 & kMIDILBitMask) >> 4;
        }
    }
    else if(layout == drumRackLayout) {
        userInput.isGridButton = !(userInput.isRightButton = message.arg1 > 99);
        
        if(userInput.isGridButton) {
            userInput.x = (userInput.y = -1);
            userInput.btnNumber = message.arg1 - 36;
        }
        else { // right button
            userInput.x = (userInput.y = -1);
            userInput.btnNumber = message.arg1 - 100;
        }
    }
    
    return userInput;
}

VstMidiEvent* LaunchPadMessagesHelper::createRawMessage(MIDIMessage const& message, 
                                                        VstInt32 deltaFrames)
{
    VstMidiEvent* midiEvent = new VstMidiEvent;
    
    midiEvent->type = kVstMidiType;
    midiEvent->byteSize = SIZEOFMIDIEVENT;
    midiEvent->deltaFrames = deltaFrames;
    midiEvent->flags = kVstMidiEventIsRealtime;
    midiEvent->midiData[0] = message.code;
    midiEvent->midiData[1] = message.arg1;
    midiEvent->midiData[2] = message.arg2;
    
    return midiEvent;
}

VstMidiEvent* LaunchPadMessagesHelper::createResetMessage(VstInt32 deltaFrames)
{
    return createRawMessage(MIDIMessage(kMIDIControllerChangeEvent, 0, 0), deltaFrames);
}

VstMidiEvent* LaunchPadMessagesHelper::createSetLayoutMessage(LaunchPadLayout layout, 
                                                              VstInt32 deltaFrames)
{
    return createRawMessage(MIDIMessage(kMIDIControllerChangeEvent, 0, char(layout)), deltaFrames);
}

VstMidiEvent* LaunchPadMessagesHelper::createChangeDirectionButtonMessage(VstInt32 direction, 
                                                                          LaunchPadLEDColor color,
                                                                          VstInt32 deltaFrames) 
{
    assert(direction >= 0 && direction < 4);
    
    return createRawMessage(MIDIMessage(kMIDIControllerChangeEvent, 
                                        104 + direction, 
                                        color), deltaFrames);
}

VstMidiEvent* LaunchPadMessagesHelper::createChangeGridButtonMessage(VstInt32 x, 
                                                                     VstInt32 y,
                                                                     LaunchPadLEDColor color,
                                                                     VstInt32 deltaFrames)
{
    assert(x >= 0 && x < 8 && y >= 0 && y< 8);
    
    return createRawMessage(MIDIMessage(kMIDINoteOnEvent, 
                                        16 * y + x, 
                                        color), deltaFrames);
}

VstMidiEvent* LaunchPadMessagesHelper::createSwapBuffersMessage(bool primaryBuffer, 
                                                                VstInt32 deltaFrames)
{
    return createRawMessage(MIDIMessage(kMIDIControllerChangeEvent, 
                                        0, 
                                        primaryBuffer ? kMIDIPrimaryBufferAdress : 
                                        kMIDISecondaryBufferAdress), deltaFrames);
}