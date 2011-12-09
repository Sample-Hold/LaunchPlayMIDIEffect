//
//  LaunchPlaySequencer.cpp
//  LaunchPlayVST
//
//  Created by Fred G on 07/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include "LaunchPlaySequencer.h"

#include <math.h>

#define BEATSPERSAMPLE(tempo, sampleRate) (tempo) / (sampleRate) / 60.0

using namespace LaunchPlayVST;

#pragma mark createEffectInstance
#if defined (LAUNCHPLAY_SEQUENCER_EXPORT)
AudioEffect* createEffectInstance (audioMasterCallback audioMaster)
{
    return new LaunchPlaySequencer(audioMaster); 
}
#endif

#pragma mark SequencerBase
SequencerBase::~SequencerBase() 
{
    
}

#pragma mark GridSequencer
GridSequencer::GridSequencer(size_t width, size_t height)
{
    boundaries_.x = width;
    boundaries_.y = height;
}

GridSequencer::~GridSequencer()
{
    
}

bool GridSequencer::EqualLocations::operator()(Worker const& a, Worker const& b) const 
{
    return a.x == b.x && a.y == b.y;
}

void GridSequencer::MoveForward::operator()(Worker & worker, Worker const& boundaries) const
{
    switch (worker.direction) {
        case up:
            worker.y = (worker.y == 0) ? boundaries.y : worker.y - 1;
            break;
        case down:
            worker.y = (worker.y >= boundaries.y) ? 0 : worker.y + 1;
            break;
        case left:
            worker.x = (worker.x == 0) ? boundaries.x : worker.x - 1;
            break;
        case right:
            worker.x = (worker.x >= boundaries.x) ? 0 : worker.x + 1;
            break;
    }
}

void GridSequencer::HandleColision::operator()(Worker & worker, WorkerList const& workerList) const
{
    // TODO Handle possible infinite loop here
    
    if(std::count_if(workerList.begin(), 
                     workerList.end(), 
                     std::bind2nd(EqualLocations(), worker)) > 0)
    {
        worker.direction = 
        (worker.direction == up) ? down :
        (worker.direction == down) ? up :
        (worker.direction == left) ? right : left;
    }
}

WorkerListIter GridSequencer::beginIterator()
{
    return workers_.begin();
}

WorkerListIter GridSequencer::endIterator()
{
    return workers_.end();
}

bool GridSequencer::addWorker(size_t posX, size_t posY, GridDirection direction)
{
    assert(posX <= boundaries_.x && posY <= boundaries_.y);
    
    Worker newWorker;
    newWorker.x = posX; newWorker.y = posY; newWorker.direction = direction;

    if(workers_.empty() ||
       std::count_if(workers_.begin(), 
                    workers_.end(), 
                    std::bind2nd(EqualLocations(), newWorker)) == 0)
    {
        workers_.push_back(newWorker);
        return true;
    }
    
    return false;
}

bool GridSequencer::removeWorker(size_t posX, size_t posY)
{
    assert(posX <= boundaries_.x && posY <= boundaries_.y);
    
    Worker workerToRemove;
    workerToRemove.x = posX; workerToRemove.y = posY;
    
    if(workers_.empty())
        return false;
    
    WorkerListIter it = std::find_if(workers_.begin(), workers_.end(), std::bind2nd(EqualLocations(), workerToRemove));
    
    if(it != workers_.end()) {
        workers_.erase(it);
        return true;
    }
    
    return false;
}

bool GridSequencer::removeAllWorkers() 
{
    if(!workers_.empty()) {
        workers_.clear();
        return true;
    }
    return false;
}

void GridSequencer::processForward(double ppq, VstInt32 sampleOffset)
{
    // move all workers
    for_each(workers_.begin(), workers_.end(), std::bind2nd(MoveForward(), boundaries_));
    
    // detect collisions
    for_each(workers_.begin(), workers_.end(), std::bind2nd(HandleColision(), workers_));      
}

VstEventsArray* GridSequencer::flushMidiEvents()
{
    // TODO implement
    return NULL;
}

#pragma mark LaunchPadSequencer
LaunchPadSequencer::LaunchPadSequencer()
: GridSequencer(kLaunchPadWidth, kLaunchPadHeight)
{
    
}

LaunchPadSequencer::~LaunchPadSequencer()
{
    
}

void LaunchPadSequencer::resetLaunchPad(VstInt32 deltaFrames)
{
    primaryBufferEnabled_ = false;
    internalEvents_.push_back((VstEvent*) LaunchPadMessagesHelper::createResetMessage(deltaFrames));
}

void LaunchPadSequencer::setXYLayout(VstInt32 deltaFrames)
{
    internalEvents_.push_back((VstEvent*) LaunchPadMessagesHelper::createSetLayoutMessage(xYLayout, 
                                                                                          deltaFrames));
}

void LaunchPadSequencer::setDirection(GridDirection direction, VstInt32 deltaFrames) 
{
    currentDirection_ = direction;
    
    for (VstInt32 i = 0; i < 4; ++i) {
        if(i == direction)
            internalEvents_.push_back((VstEvent*) 
                                      LaunchPadMessagesHelper::createChangeDirectionButtonMessage(i, 
                                                                                                  fullYellow,
                                                                                                  deltaFrames));
        else
            internalEvents_.push_back((VstEvent*)
                                      LaunchPadMessagesHelper::createChangeDirectionButtonMessage(i, 
                                                                                                  off,
                                                                                                  deltaFrames));
    }
}

void LaunchPadSequencer::swapBuffers(VstInt32 deltaFrames)
{
    internalEvents_.push_back((VstEvent*) LaunchPadMessagesHelper::createSwapBuffersMessage(primaryBufferEnabled_, 
                                                                                            deltaFrames));
    primaryBufferEnabled_ = !primaryBufferEnabled_;
}

void LaunchPadSequencer::init()
{
    resetLaunchPad(0);
    setXYLayout(1);
    swapBuffers(1);
    setDirection(up, 2);
    swapBuffers(3);
}

void LaunchPadSequencer::processForward(double ppq, VstInt32 sampleOffset) 
{
    // clear current workers
    for (WorkerListIter it = beginIterator(); it != endIterator(); ++it)
        internalEvents_.push_back((VstEvent*) LaunchPadMessagesHelper::createChangeGridButtonMessage(it->x, 
                                                                                                     it->y, 
                                                                                                     off,
                                                                                                     sampleOffset));
    GridSequencer::processForward(ppq, sampleOffset);
    
    // show updated workers
    for (WorkerListIter it = beginIterator(); it != endIterator(); ++it)
        internalEvents_.push_back((VstEvent*) LaunchPadMessagesHelper::createChangeGridButtonMessage(it->x, 
                                                                                                     it->y, 
                                                                                                     fullRed,
                                                                                                     sampleOffset));
    
    swapBuffers(sampleOffset + 1);
}

void LaunchPadSequencer::processUserEvents(VstEvents *eventPtr)
{
    for (VstInt32 i = 0; i < eventPtr->numEvents; ++i) 
    {
        VstEvent *event = eventPtr->events[i]; 
        
        if(event->type != kVstMidiType)
            continue;
        
        VstMidiEvent *midiEvent = (VstMidiEvent*) event;        
        if(LaunchPadMessagesHelper::isValidMessage(midiEvent)) {
            LaunchPadUserInput userInput = LaunchPadMessagesHelper::readMessage(midiEvent, xYLayout);
            VstInt32 deltaFrames = midiEvent->deltaFrames;
            
            if(userInput.isTopButton && userInput.btnPressed && userInput.btnNumber < 5) {
                setDirection(GridDirection(userInput.btnNumber), deltaFrames);
                continue;
            }
            
            if(userInput.isGridButton && userInput.btnReleased) {
                addWorker(userInput.x, userInput.y, currentDirection_);
                continue;
            }
        }
    }
}

VstEventsArray* LaunchPadSequencer::flushFeedbackEvents()
{
    VstEventsArray *events = VstMIDIHelper::allocateVstEventsArray(internalEvents_.size());
    
    for(size_t i = 0; i < internalEvents_.size(); ++i)
        events->events[i] = internalEvents_[i];
    
    internalEvents_.clear();
    
    return events;
}

#pragma mark LaunchPlaySequencer
LaunchPlaySequencer::LaunchPlaySequencer(audioMasterCallback audioMaster)
: LaunchPlayBase(audioMaster, 0, 0)
{ 
    setUniqueID(kSeqUniqueID);
    noTail();
    isSynth();
}

LaunchPlaySequencer::~LaunchPlaySequencer()
{
    
}

bool LaunchPlaySequencer::getEffectName(char* name)
{
	vst_strncpy (name, kSequencerName, kVstMaxEffectNameLen);
	return true;
}

VstInt32 LaunchPlaySequencer::canDo(char *text)
{
    if(strcmp(text, "sendVstMidiEvent") == 0 || 
       strcmp(text, "receiveVstMidiEvent") == 0 ||
       strcmp(text, "receiveVstTimeInfo") == 0)
        return 1;
    
	if(strcmp(text, "offline") == 0)
        return -1;
    
    return 0;
}    

void LaunchPlaySequencer::detectTicks(VstTimeInfo *timeInfo, 
                                 VstInt32 sampleFrames, 
                                 double stride)
{
    double startPpqPos = timeInfo->ppqPos;
    double beatsPerSample = BEATSPERSAMPLE(timeInfo->tempo, timeInfo->sampleRate);
    double endPpqPos = startPpqPos + sampleFrames * beatsPerSample;
    
    double ppqPos = ceil(startPpqPos);
    while (ppqPos - stride > startPpqPos)
        ppqPos -= stride;
    
    for(;ppqPos < endPpqPos; ppqPos += stride)
        onTick(ppqPos, VstInt32((ppqPos - startPpqPos) / beatsPerSample));
}

void LaunchPlaySequencer::onTick(double ppq, VstInt32 sampleOffset)
{
    assert(sequencer_.get() != NULL);
    
    // flush previous feedback events
    VstEventsArray* feedbackEvents = sequencer_->flushFeedbackEvents();
    
    // weird cast but obviously the VST SDK gives me no choice
    VstEvents* events = (VstEvents*) feedbackEvents;
    
    // send back feedback events to host - buggy
    //sendVstEventsToHost(events);
    
    // clean memory
    VstMIDIHelper::freeVstEventsArray(feedbackEvents);
    
    // move sequencer forward
    sequencer_->processForward(ppq, sampleOffset);
    
    // flush generated events
    feedbackEvents = sequencer_->flushFeedbackEvents();
    
    // again this weird devilish cast
    events = (VstEvents*) feedbackEvents;
    
    // don't wait next turn to send generated feedback events back to host - buggy
    //sendVstEventsToHost(events);
    
    // clean memory
    VstMIDIHelper::freeVstEventsArray(feedbackEvents);
}

void LaunchPlaySequencer::open()
{
    sequencer_.reset(new LaunchPadSequencer);
    sequencer_->init();
}

void LaunchPlaySequencer::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
    VstTimeInfo *time = getTimeInfo(kVstTransportChanged | kVstTransportPlaying | kVstTempoValid | kVstPpqPosValid);
    
    if (time 
        && time->flags & kVstTransportPlaying
        && time->flags & kVstTempoValid
        && time->flags & kVstPpqPosValid)
    {
        detectTicks(time, sampleFrames);
    }
}

VstInt32 LaunchPlaySequencer::processEvents(VstEvents *eventPtr) 
{
    assert(sequencer_.get() != NULL);
    
    sequencer_->processUserEvents(eventPtr);
    return 0;
}