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
     
    code = event->midiData[0];
    arg1 = event->midiData[1];
    arg2 = event->midiData[2];
}

MIDIMessage::MIDIMessage(char c, char a1, char a2)
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
    for(VstInt32 i = 0; i < this->numEvents; ++i)
        delete this->events[i];
    
    this->numEvents = 0;
}

void VstEventsBlock::convertMidiEvent(VstMidiEvent *source, VstEvent *event)
{
    assert(source != NULL && event != NULL);
    
    VstMidiEvent *dest = (VstMidiEvent*) event;
    
    dest->type = source->type;
    dest->flags = source->flags;
    dest->byteSize = source->byteSize;
    dest->deltaFrames = source->deltaFrames;
    dest->noteLength = source->noteLength;
    dest->noteOffset = source->noteOffset;
    dest->noteOffVelocity = source->noteOffVelocity;
    dest->detune = source->detune;
    
	memcpy(dest->midiData, source->midiData, sizeof(char)*4);
}

void VstEventsBlock::filterMidiEvents(VstEvents *events, char channelOffset)
{
	assert(events != NULL);

	for(VstInt32 i = 0; i < events->numEvents; ++i) {
		VstEvent *event = events->events[i];

		if(event->type != kVstMidiType)
			continue;
		
		VstMidiEvent *midiEvent = (VstMidiEvent*) event;
		char eventChannel = midiEvent->midiData[0] & kMIDIRBitMask;

		if(eventChannel != channelOffset) // mute midi event
			memset(midiEvent->midiData, 0, 4);
	}
}

void VstEventsBlock::debugVstEvents(VstEvents const* events, char midiEventToWatch)
{
    assert(events != NULL);
    
    for(VstInt32 i=0; i < events->numEvents; ++i) {
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
            printf("VstMidiEvent midiData[%u]=0x%X\n", (VstInt32) j, midiEvent->midiData[j] & kMIDILRBitMask);
        
        printf("\n");
    }
}

#pragma mark VstDelayedMidiEvent
VstDelayedMidiEvent::VstDelayedMidiEvent(VstMidiEventPtr const event, double ppq)
{
	assert(event != NULL);

	type = event->type;
	byteSize = event->byteSize;
	flags = event->flags;
	noteLength = event->noteLength;
	noteOffset = event->noteOffset;
	detune = event->detune;
	noteOffVelocity = event->noteOffVelocity;

	memcpy(midiData, event->midiData, sizeof(char)*4);
	ppqStart = ppq;
}

VstMidiEventPtr VstDelayedMidiEvent::toMidiEvent(VstInt32 frames)
{
	VstMidiEventPtr midiEvent(new VstMidiEvent); // create shared_ptr copy

	midiEvent->type = type;
	midiEvent->byteSize = byteSize;
	midiEvent->flags = flags;
	midiEvent->noteLength = noteLength;
	midiEvent->noteOffset = noteOffset;
	midiEvent->detune = detune;
	midiEvent->noteOffVelocity = noteOffVelocity;
	midiEvent->deltaFrames = frames;
	memcpy(midiEvent->midiData, midiData, sizeof(char)*4);

	return midiEvent;
}

#pragma mark MIDIHelper
const int MIDIHelper::logScaleMajorOffsets[]		= {0, 2, 4, 5, 7, 9, 11, 12};
const int MIDIHelper::logScaleMinorNatOffsets[]		= {0, 2, 3, 5, 7, 8, 10, 12};  
const int MIDIHelper::logScaleMinorHarOffsets[]		= {0, 2, 3, 5, 7, 8, 11, 12};
const int MIDIHelper::logScaleMinorMelAscOffsets[]	= {0, 2, 3, 5, 7, 9, 11, 12}; 
const int MIDIHelper::logScaleMinorMelDescOffsets[]	= {0, 2, 3, 5, 7, 8, 11, 12}; 
const int MIDIHelper::pentScaleMajorOffsets[]		= {0, 2, 4, 7, 9, 12, 14, 16}; 
const int MIDIHelper::pentScaleMinorNatOffsets[]	= {0, 3, 5, 7, 10, 12, 15, 17}; 
const int MIDIHelper::hexScaleWholeOffsets[]		= {0, 2, 4, 6, 8, 10, 12, 14}; 
const int MIDIHelper::hexScaleAugOffsets[]			= {0, 3, 4, 7, 8, 11, 12, 15}; 
const int MIDIHelper::hexScalePromOffsets[]			= {0, 2, 4, 6, 9, 10, 12, 14}; 
const int MIDIHelper::hexScaleBluesOffsets[]		= {0, 3, 5, 6, 7, 10, 12, 15}; 
const int MIDIHelper::hexScaleTritoneOffsets[]		= {0, 1, 4, 6, 7, 10, 12, 13};

VstMidiEventPtr MIDIHelper::createNoteOn(VstInt32 baseNoteOffset, 
									  VstInt32 octave,
                                      Scale scale, 
                                      VstInt32 noteOffset, 
                                      VstInt32 deltaFrames,
									  char channelOffset)
{
    assert(baseNoteOffset >= 0 && 
		baseNoteOffset < kBaseNoteCount &&
		octave >= kBaseNoteMinOctave &&
		octave <= kBaseNoteMaxOctave &&
		noteOffset >= 0 && 
		noteOffset <= kNoteOffsetMaxValue && 
		channelOffset >= 0 && 
		channelOffset < kMaxChannels);
    
    VstMidiEventPtr midiEvent(new VstMidiEvent);
    char note(kMIDIMiddleCNoteNumber + baseNoteOffset);
    
	note += octave*kBaseNoteCount;

    switch (scale) {
        case logScaleMajor:
            note += logScaleMajorOffsets[noteOffset];
            break;
        case logScaleMinorNat:
            note += logScaleMinorNatOffsets[noteOffset];
            break;
		case logScaleMinorHar:
            note += logScaleMinorHarOffsets[noteOffset];
            break;
        case logScaleMinorMelAsc:
            note += logScaleMinorMelAscOffsets[noteOffset];
            break;
		case logScaleMinorMelDesc:
            note += logScaleMinorMelDescOffsets[noteOffset];
            break;
		case pentScaleMajor:
            note += pentScaleMajorOffsets[noteOffset];
            break;
		case pentScaleMinorNat:
            note += pentScaleMinorNatOffsets[noteOffset];
            break;
		case hexScaleWhole:
            note += hexScaleWholeOffsets[noteOffset];
            break;
		case hexScaleAug:
            note += hexScaleAugOffsets[noteOffset];
            break;
		case hexScaleProm:
            note += hexScalePromOffsets[noteOffset];
            break;
		case hexScaleBlues:
            note += hexScaleBluesOffsets[noteOffset];
            break;
		case hexScaleTritone:
            note += hexScaleTritoneOffsets[noteOffset];
            break;
    }

    midiEvent->type = kVstMidiType;
    midiEvent->byteSize = SIZEOFMIDIEVENT;
    midiEvent->deltaFrames = deltaFrames;
    midiEvent->flags = 0;
    midiEvent->midiData[0] = (char) kMIDINoteOnEvent | channelOffset;
    midiEvent->midiData[1] = note;
    midiEvent->midiData[2] = (char) kMIDIVelocityGood;
    midiEvent->noteLength = (midiEvent->noteOffset = (midiEvent->noteOffVelocity = (midiEvent->detune = 0)));

    return midiEvent;
}

VstMidiEventPtr MIDIHelper::createNoteOff(VstInt32 baseNoteOffset, 
										VstInt32 octave,
										Scale scale, 
										VstInt32 noteOffset, 
										VstInt32 deltaFrames,
										char channelOffset)
{
    
     VstMidiEventPtr midiEvent = createNoteOn(baseNoteOffset, 
									       octave,
                                           scale, 
                                           noteOffset, 
										   deltaFrames,
										   channelOffset);
    
    midiEvent->midiData[0] = (char) kMIDINoteOffEvent | channelOffset;
    midiEvent->midiData[2] = (char) kMIDIVelocityMin;
    return midiEvent;
}

#pragma mark LaunchPadHelper
bool LaunchPadHelper::isValidMessage(MIDIMessage const& message)
{
    return (message.code & char(kMIDILBitMask)) == char(kMIDINoteOnEvent) ||
           (message.code & char(kMIDILBitMask)) == char(kMIDINoteOffEvent) ||
           (message.code & char(kMIDILBitMask)) == char(kMIDIControllerChangeEvent);
}

LaunchPadUserInput LaunchPadHelper::readMessage(MIDIMessage const& message, 
                                                LaunchPadLayout layout)
{
    LaunchPadUserInput userInput;
    
    userInput.btnPressed = (message.arg2 == kMIDIVelocityMax);
	userInput.btnReleased = (message.arg2 == kMIDIVelocityMin || message.arg2 == kMIDIVelocityAverage);
    userInput.isTopButton = ((message.code  & char(kMIDILBitMask)) == char(kMIDIControllerChangeEvent));
    
    if (userInput.isTopButton) {  
        userInput.isGridButton = (userInput.isRightButton = false);
        userInput.x = (userInput.y = -1);
        userInput.btnNumber = message.arg1 - 104;
        
        return userInput;
    }
    
    if(layout == xYLayout) {
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

VstMidiEventPtr LaunchPadHelper::createRawMessage(MIDIMessage const& message, 												
												VstInt32 deltaFrames,
												char flags)
{
    VstMidiEventPtr midiEvent(new VstMidiEvent);
    
    midiEvent->type = kVstMidiType;
    midiEvent->byteSize = SIZEOFMIDIEVENT;
    midiEvent->deltaFrames = deltaFrames;
    midiEvent->flags = 0;
    midiEvent->midiData[0] = message.code;
    midiEvent->midiData[1] = message.arg1;
    midiEvent->midiData[2] = message.arg2 | flags;
    midiEvent->noteLength = (midiEvent->noteOffset = (midiEvent->noteOffVelocity = (midiEvent->detune = 0)));
    
    return midiEvent;
}

VstMidiEventPtr LaunchPadHelper::createResetMessage(VstInt32 deltaFrames)
{
    return createRawMessage(MIDIMessage(char(kMIDIControllerChangeEvent), 0, 0), deltaFrames);
}

VstMidiEventPtr LaunchPadHelper::createSetLayoutMessage(LaunchPadLayout layout, 
                                                     VstInt32 deltaFrames)
{
    return createRawMessage(MIDIMessage(char(kMIDIControllerChangeEvent), 0, char(layout)), deltaFrames);
}

VstMidiEventPtr LaunchPadHelper::createTopButtonMessage(VstInt32 number, 
                                                   LaunchPadLEDColor color,
                                                   VstInt32 deltaFrames, 
												   char flags) 
{
    assert(number >= 0 && number < 8);
    
    return createRawMessage(MIDIMessage(char(kMIDIControllerChangeEvent), 
                                        char(104 + number), 
                                        char(color)),
                            deltaFrames,
							flags);
}

VstMidiEventPtr LaunchPadHelper::createGridButtonMessage(size_t x, 
                                                    size_t y,
                                                    LaunchPadLEDColor color,
                                                    VstInt32 deltaFrames,
													char flags)
{
    assert(x >= 0 && x < 8 && y >= 0 && y< 8);
    
    char noteNumber = 16 * y + x;
    return createRawMessage(MIDIMessage(char(kMIDINoteOnEvent), 
                                        noteNumber, 
                                        char(color)), 
                            deltaFrames,
							flags);
}

VstMidiEventPtr LaunchPadHelper::createRightButtonMessage(VstInt32 number,
														LaunchPadLEDColor color,
														VstInt32 deltaFrames,
														char flags)
{
	assert(number >= 0 && number < 8);

	return createRawMessage(MIDIMessage(char(kMIDINoteOnEvent), 
                                        char((number << 4) + 8), 
                                        char(color)), 
                            deltaFrames,
                            flags);
}

VstMidiEventPtr LaunchPadHelper::createSwapBuffersMessage(bool primaryBuffer, 
                                                       VstInt32 deltaFrames)
{
    return createRawMessage(MIDIMessage(char(kMIDIControllerChangeEvent), 
                                        0, 
                                        primaryBuffer ? 
                                        char(kMIDIPrimaryBufferCode) : 
                                        char(kMIDISecondaryBufferCode)), 
                            deltaFrames);
}