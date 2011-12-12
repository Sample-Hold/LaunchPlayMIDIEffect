//
//  MIDIHelper.cpp
//  LaunchPlayVST
//
//  Created by Fred G on 08/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include "MIDIHelpers.h"

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

#pragma mark VstEventsBlock
void VstEventsBlock::allocate(size_t const size)
{
    assert(size <= kVstEventsBlockSize);
    
    this->numEvents = VstInt32(size);
    
    for(size_t i = 0; i < size; ++i)
        this->events[i] = new VstEvent;
}

void VstEventsBlock::deallocate()
{
    for(size_t i = 0; i < this->numEvents; ++i)
        delete this->events[i];
    
    this->numEvents = 0;
}

void VstEventsBlock::convertMidiEvent(VstMidiEvent const& midiEvent, VstEvent *event)
{
    assert(event != NULL);
    
    VstMidiEvent *dest = (VstMidiEvent*) event;
    
    dest->type = midiEvent.type;
    dest->flags = midiEvent.flags;
    dest->byteSize = midiEvent.byteSize;
    dest->deltaFrames = midiEvent.deltaFrames;
    dest->noteLength = midiEvent.noteLength;
    dest->noteOffset = midiEvent.noteOffset;
    dest->noteOffVelocity = midiEvent.noteOffVelocity;
    dest->detune = midiEvent.detune;
    
    for(size_t j=0; j<3; ++j)
        dest->midiData[j] = midiEvent.midiData[j] & kMIDILRBitMask;
}

void VstEventsBlock::debugVstEvents(VstEvents const* events, char midiEventToWatch)
{
    assert(events != NULL);
    
    for(size_t i=0; i < events->numEvents; ++i) {
        VstEvent* event = events->events[i];
    
        if (event->type != kVstMidiType)
            return;
        
        VstMidiEvent* midiEvent = (VstMidiEvent*) event;
        
        if(midiEventToWatch != 0 && midiEvent->midiData[0] != midiEventToWatch)
            continue;
        
        printf("VstMidiEvent flags=%d\n", midiEvent->flags);
        printf("VstMidiEvent byteSize=%d\n", midiEvent->byteSize);
        printf("VstMidiEvent deltaFrames=%d\n", midiEvent->deltaFrames);

        for(size_t j=0; j<3; ++j)
            printf("VstMidiEvent midiData[%u]=0x%X\n", (uint32_t) j, midiEvent->midiData[j] & kMIDILRBitMask);
        
        printf("\n");
    }
}

#pragma mark MIDIHelper
const int MIDIHelper::majorScaleOffsets[] = {0, 2, 4, 5, 7, 9, 11, 12};
const int MIDIHelper::minorScaleOffsets[] = {0, 2, 3, 5, 7, 8, 10, 12};

VstMidiEvent MIDIHelper::createNoteOn(VstInt32 baseNoteOffset, 
                                    Scale scale, 
                                    VstInt32 offset, 
                                    VstInt32 deltaFrames)
{
    assert(offset >= 0 && offset < 8);
    
    VstMidiEvent midiEvent;
    VstInt32 note(kMIDIMiddleCNoteNumber + baseNoteOffset);
    
    switch (scale) {
        case majorScale:
            note += majorScaleOffsets[offset];
            break;
        case minorScale:
            note += minorScaleOffsets[offset];
            break;
    }
    
    midiEvent.type = kVstMidiType;
    midiEvent.byteSize = SIZEOFMIDIEVENT;
    midiEvent.deltaFrames = deltaFrames;
    midiEvent.flags = kVstMidiEventIsRealtime;
    midiEvent.midiData[0] = kMIDINoteOnEvent;
    midiEvent.midiData[1] = (unsigned char) note;
    midiEvent.midiData[2] = kMIDIVelocityMax;
    midiEvent.noteLength = (midiEvent.noteOffset = (midiEvent.noteOffVelocity = (midiEvent.detune = 0)));

    return midiEvent;
}

VstMidiEvent MIDIHelper::createNoteOff(VstInt32 baseNoteOffset, 
                                    Scale scale, 
                                    VstInt32 offset, 
                                    VstInt32 deltaFrames)
{
    
     VstMidiEvent midiEvent = createNoteOn(baseNoteOffset, 
                                           scale, 
                                           offset, 
                                           deltaFrames);
    
    midiEvent.midiData[0] = kMIDINoteOffEvent;
    midiEvent.midiData[2] = kMIDIVelocityMin;
    return midiEvent;
}

#pragma mark LaunchPadHelper
bool LaunchPadHelper::isValidMessage(VstMidiEvent const* event)
{
    assert(event != NULL);
    
    MIDIMessage message(event);
    
    return (message.code & kMIDILBitMask) == kMIDINoteOnEvent ||
           (message.code & kMIDILBitMask) == kMIDINoteOffEvent ||
           (message.code & kMIDILBitMask) == kMIDIControllerChangeEvent;
}

LaunchPadUserInput LaunchPadHelper::readMessage(VstMidiEvent const* event, 
                                                LaunchPadLayout layout)
{
    assert(event->type == kVstMidiType);
    
    MIDIMessage message(event);
    LaunchPadUserInput userInput;
    
    userInput.btnPressed = (message.arg2 == kMIDIVelocityMax);
    userInput.isTopButton = (message.code == kMIDIControllerChangeEvent);
    
    if (userInput.isTopButton) {
        userInput.btnReleased = (message.arg2 == kMIDIVelocityMin);
        userInput.isGridButton = (userInput.isRightButton = false);
        userInput.x = (userInput.y = -1);
        userInput.btnNumber = message.arg1 - 104;
        
        return userInput;
    }
    
    if(layout == xYLayout) {
        userInput.btnReleased = (message.arg2 == kMIDIVelocityAverage);
        userInput.isGridButton = !(userInput.isRightButton = 
                                   (message.arg1 & kMIDILBitMask) >> 4 < 8 &&
                                   (message.arg1 & kMIDIRBitMask) == 8);
        
        if(userInput.isGridButton) {
            userInput.x = (message.arg1 & kMIDIRBitMask);
            userInput.y = (message.arg1 & kMIDILBitMask) >> 4;
            userInput.btnNumber = userInput.y * 8 + userInput.x;
            
            return userInput;
        }
        
        // right button
        userInput.x = (userInput.y = -1);
        userInput.btnNumber = (message.arg1 & kMIDILBitMask) >> 4;
        return userInput;
    }
        
    if(layout == drumRackLayout) {
        userInput.btnReleased = (message.arg2 == kMIDIVelocityAverage);
        userInput.isGridButton = !(userInput.isRightButton = message.arg1 > 99);
        
        if(userInput.isGridButton) {
            userInput.x = (userInput.y = -1);
            userInput.btnNumber = message.arg1 - 36;
            return userInput;
        }
        
        // right button
        userInput.x = (userInput.y = -1);
        userInput.btnNumber = message.arg1 - 100;
    }
    
    return userInput;
}

VstMidiEvent LaunchPadHelper::createRawMessage(MIDIMessage const& message, 
                                               bool copyBit,
                                               bool clearBit, 
                                               VstInt32 deltaFrames)
{
    VstMidiEvent midiEvent;
    
    midiEvent.type = kVstMidiType;
    midiEvent.byteSize = SIZEOFMIDIEVENT;
    midiEvent.deltaFrames = deltaFrames;
    midiEvent.flags = kVstMidiEventIsRealtime;
    midiEvent.midiData[0] = message.code;
    midiEvent.midiData[1] = message.arg1;
    midiEvent.midiData[2] = message.arg2;
    midiEvent.noteLength = (midiEvent.noteOffset = (midiEvent.noteOffVelocity = (midiEvent.detune = 0)));
    
    if(copyBit)
        midiEvent.midiData[2] += 4;
    
    if(clearBit)
        midiEvent.midiData[2] += 8;
    
    return midiEvent;
}

VstMidiEvent LaunchPadHelper::createResetMessage(VstInt32 deltaFrames)
{
    return createRawMessage(MIDIMessage(kMIDIControllerChangeEvent, 0, 0), 
                            false,
                            false,
                            deltaFrames);
}

VstMidiEvent LaunchPadHelper::createSetLayoutMessage(LaunchPadLayout layout, 
                                                     VstInt32 deltaFrames)
{
    return createRawMessage(MIDIMessage(kMIDIControllerChangeEvent, 0, char(layout)), 
                            false,
                            false,
                            deltaFrames);
}

VstMidiEvent LaunchPadHelper::createTopButtonMessage(VstInt32 number, 
                                                     LaunchPadLEDColor color,
                                                     VstInt32 deltaFrames) 
{
    assert(number >= 0 && number < 8);
    
    return createRawMessage(MIDIMessage(kMIDIControllerChangeEvent, 
                                        104 + number, 
                                        color), 
                            true,
                            false,
                            deltaFrames);
}

VstMidiEvent LaunchPadHelper::createGridButtonMessage(size_t x, 
                                                      size_t y,
                                                      LaunchPadLEDColor color,
                                                      VstInt32 deltaFrames)
{
    assert(x >= 0 && x < 9 && y >= 0 && y< 8);
    
    unsigned char noteNumber = 16 * y + x;
    return createRawMessage(MIDIMessage(kMIDINoteOnEvent, 
                                        noteNumber, 
                                        color), 
                            true,
                            false,
                            deltaFrames);
}

VstMidiEvent LaunchPadHelper::createSwapBuffersMessage(bool primaryBuffer, 
                                                       VstInt32 deltaFrames)
{
    return createRawMessage(MIDIMessage(kMIDIControllerChangeEvent, 
                                        0, 
                                        primaryBuffer ? 
                                        kMIDIPrimaryBufferAdress : 
                                        kMIDISecondaryBufferAdress), 
                            false, 
                            false,
                            deltaFrames);
}