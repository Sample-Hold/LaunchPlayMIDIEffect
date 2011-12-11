//
//  LaunchPlaySequencer.cpp
//  LaunchPlayVST
//
//  Created by Fred G on 07/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include "LaunchPlaySequencer.h"

#include <math.h>
#include <time.h>

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

void SequencerBase::sendMidiEventsToHost(LaunchPlayBase *plugin)
{
    assert(plugin != NULL);
    
    VstEventsBlock eventsBlock;
    eventsBlock.allocate(midiEventsCount());
    flushMidiEvents(eventsBlock);
    
    VstEvents *events = (VstEvents*) &eventsBlock;
    plugin->sendVstEventsToHost(events);
    
    eventsBlock.deallocate(); 
}

void SequencerBase::sendFeedbackEventsToHost(LaunchPlayBase *plugin)
{
    assert(plugin != NULL);
    
    VstEventsBlock eventsBlock;
    eventsBlock.allocate(feedbackEventsCount());
    flushFeedbackEvents(eventsBlock);
    
    VstEvents *events = (VstEvents*) &eventsBlock;
    plugin->sendVstEventsToHost(events);
    
    eventsBlock.deallocate();
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
    return a.uniqueID != b.uniqueID && a.x == b.x && a.y == b.y;
}

void GridSequencer::MoveForward::operator()(Worker & worker, Worker const& boundaries) const
{
    switch (worker.direction) {
        case up:
            worker.y = (worker.y == 0) ? boundaries.y - 1 : worker.y - 1;
            break;
        case down:
            worker.y = (worker.y >= boundaries.y - 1) ? 0 : worker.y + 1;
            break;
        case left:
            worker.x = (worker.x == 0) ? boundaries.x - 1 : worker.x - 1;
            break;
        case right:
            worker.x = (worker.x >= boundaries.x - 1) ? 0 : worker.x + 1;
            break;
    }
}

void GridSequencer::HandleCollisions::operator()(Worker & worker, WorkerList const& workerList) const
{
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

WorkerListIter GridSequencer::getWorkersBeginIterator()
{
    return workers_.begin();
}

WorkerListIter GridSequencer::getWorkersEndIterator()
{
    return workers_.end();
}

bool GridSequencer::addWorker(Worker const& worker)
{
    assert(worker.x < boundaries_.x && worker.y < boundaries_.y);
    
    if(workers_.empty() || countWorkersAtLocation(worker) == 0)
    {
        workers_.push_back(worker);
        return true;
    }
    
    return false;
}

bool GridSequencer::removeWorkers(Worker const& worker)
{
    assert(worker.x < boundaries_.x && worker.y < boundaries_.y);
    
    if (workers_.empty())
        return false;

    bool workersWereRemoved = false;
    WorkerListIter it;
    while((it = std::find_if(workers_.begin(), 
                          workers_.end(), 
                          std::bind2nd(EqualLocations(), worker))) != workers_.end())
    {
        workersWereRemoved = true;
        workers_.erase(it);
    }
    
    return workersWereRemoved;
    
}

bool GridSequencer::removeAllWorkers() 
{
    if(workers_.empty())
        return false;
    
    workers_.clear();
    return true;
}

size_t GridSequencer::countWorkersAtLocation(Worker const& worker)
{
    size_t count = std::count_if(workers_.begin(), 
                                 workers_.end(), 
                                 std::bind2nd(EqualLocations(), worker));
     
    return count;
}

void GridSequencer::processForward(double ppq, VstInt32 sampleOffset)
{
    // move all workers
    for_each(workers_.begin(), workers_.end(), std::bind2nd(MoveForward(), boundaries_));
    
    // detect collisions
    for_each(workers_.begin(), workers_.end(), std::bind2nd(HandleCollisions(), workers_));      
}

size_t GridSequencer::midiEventsCount() const
{
    // TODO implement
    return 0;
}

void GridSequencer::flushMidiEvents(VstEventsBlock &events)
{
    // TODO implement
}

#pragma mark LaunchPadSequencer
LaunchPadSequencer::LaunchPadSequencer()
: GridSequencer(kLaunchPadWidth, kLaunchPadHeight), 
currentDirection_(up), currentEditMode_(addWorkersMode), primaryBufferEnabled_(false)
{
    
}

LaunchPadSequencer::~LaunchPadSequencer()
{
    
}

void LaunchPadSequencer::resetLaunchPad(VstInt32 deltaFrames)
{
    primaryBufferEnabled_ = false;
    eventsBuffer_.push_back(LaunchPadHelper::createResetMessage(deltaFrames));
}

void LaunchPadSequencer::setXYLayout(VstInt32 deltaFrames)
{
    eventsBuffer_.push_back(LaunchPadHelper::createSetLayoutMessage(xYLayout,
                                                                    deltaFrames));
}

void LaunchPadSequencer::cleanWorkers(VstInt32 deltaFrames)
{
    for(WorkerListIter it = getWorkersBeginIterator(); it != getWorkersEndIterator(); ++it) {
        eventsBuffer_.push_back(LaunchPadHelper::createGridButtonMessage(it->x, 
                                                                         it->y,
                                                                         off,
                                                                         deltaFrames));
    }
}

void LaunchPadSequencer::showWorkers(VstInt32 deltaFrames)
{
    for(WorkerListIter it = getWorkersBeginIterator(); it != getWorkersEndIterator(); ++it) {
        bool collision = countWorkersAtLocation(*it) > 0;
        
        
        eventsBuffer_.push_back(LaunchPadHelper::createGridButtonMessage(it->x, 
                                                                         it->y,
                                                                         collision ? 
                                                                         fullRed : fullGreen,
                                                                         deltaFrames));
    }
}

void LaunchPadSequencer::showDirections(VstInt32 deltaFrames) 
{
    for (VstInt32 i = 0; i < 4; ++i)
    {
        bool showDirection  = (i == currentDirection_);
        eventsBuffer_.push_back(LaunchPadHelper::createTopButtonMessage(i, 
                                                                        showDirection ?
                                                                        fullAmber : off,
                                                                        deltaFrames));
    }
}

void LaunchPadSequencer::showEditMode(VstInt32 deltaFrames)
{
    eventsBuffer_.push_back(LaunchPadHelper::createTopButtonMessage(4, 
                                                                    currentEditMode_ == addWorkersMode ?
                                                                    fullAmber : fullRed,
                                                                    deltaFrames));
}

void LaunchPadSequencer::showRemoveButton(VstInt32 deltaFrames)
{
    eventsBuffer_.push_back(LaunchPadHelper::createTopButtonMessage(5, 
                                                                    fullAmber,
                                                                    deltaFrames));
}

void LaunchPadSequencer::showTick(double ppq, VstInt32 deltaFrames)
{
    bool tickTack = VstInt32(floor(ppq)) % 2 == 0;
    
    eventsBuffer_.push_back(LaunchPadHelper::createTopButtonMessage(6, 
                                                                    tickTack? fullRed : off,
                                                                    deltaFrames));
    eventsBuffer_.push_back(LaunchPadHelper::createTopButtonMessage(7, 
                                                                    tickTack? off : fullRed,
                                                                    deltaFrames));
}

void LaunchPadSequencer::swapBuffers(VstInt32 deltaFrames)
{
    primaryBufferEnabled_ = !primaryBufferEnabled_;
    eventsBuffer_.push_back(LaunchPadHelper::createSwapBuffersMessage(primaryBufferEnabled_, 
                                                                              deltaFrames));
}

size_t LaunchPadSequencer::feedbackEventsCount() const
{
    return eventsBuffer_.size();
}

void LaunchPadSequencer::flushFeedbackEvents(VstEventsBlock &events)
{
    for (size_t i = 0; i < eventsBuffer_.size(); ++i)        
        VstEventsBlock::convertMidiEvent(eventsBuffer_[i], events.events[i]);

    eventsBuffer_.clear();
}

void LaunchPadSequencer::init()
{
    resetLaunchPad(0);
}

void LaunchPadSequencer::processForward(double ppq, VstInt32 sampleOffset) 
{
    // turn workers off
    cleanWorkers(sampleOffset);
    
    // check for "remove all" request
    if(removeAll_) {
        removeAllWorkers();
        removeAll_ = false;
    }
    
    // check for "remove worker" request
    if(tempWorker_.get() != NULL && currentEditMode_ == removeWorkersMode) {
        removeWorkers(*tempWorker_);
        tempWorker_.release();
    }
    
    //processForward
    GridSequencer::processForward(ppq, sampleOffset);
    
    // check for "add worker" request
    if(tempWorker_.get() != NULL && currentEditMode_ == addWorkersMode) {
        addWorker(*tempWorker_);
        tempWorker_.release();
    }
    
    // turn workers on
    showWorkers(sampleOffset);
    
     // show other buttons
    showDirections(sampleOffset);
    showEditMode(sampleOffset);
    showRemoveButton(sampleOffset);
    showTick(ppq, sampleOffset);
}

void LaunchPadSequencer::processUserEvents(VstEvents *eventPtr)
{
    for (VstInt32 i = 0; i < VstInt32(eventPtr->numEvents); ++i) 
    {
        VstEvent *event = eventPtr->events[i]; 
        
        if(event->type != kVstMidiType)
            continue;
        
        VstMidiEvent *midiEvent = (VstMidiEvent*) event;        
        if(LaunchPadHelper::isValidMessage(midiEvent)) {
            LaunchPadUserInput userInput = LaunchPadHelper::readMessage(midiEvent, xYLayout);

            if(userInput.isTopButton && userInput.btnPressed && userInput.btnNumber < 4) {
                currentDirection_ = (GridDirection) userInput.btnNumber;
                continue;
            }
            
            if(userInput.isTopButton && userInput.btnPressed && userInput.btnNumber == 4) {
                currentEditMode_ = (currentEditMode_ == addWorkersMode) ? 
                removeWorkersMode : addWorkersMode; 
                continue;
            }
            
            if(userInput.isTopButton && userInput.btnPressed && userInput.btnNumber == 5) {
                removeAll_ = true;
                continue;
            }
            
            if(userInput.isGridButton && userInput.btnReleased) {
                tempWorker_ = std::auto_ptr<Worker>(new Worker);
                tempWorker_->uniqueID = VstInt32(time(NULL));
                tempWorker_->x = userInput.x;
                tempWorker_->y = userInput.y;
                tempWorker_->direction = currentDirection_;
                continue;
            }
        }
    }
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

void LaunchPlaySequencer::onTick(double ppq, VstInt32 sampleOffset)
{
    assert(sequencer_.get() != NULL && sampleOffset >= 0);
    
    // trick to initialize LaunchPad on first frame
    if(sampleOffset == 0)
        sequencer_->sendFeedbackEventsToHost(this);
    
    // move sequencer forward; avoid 0-10 for sampleOffset, this is used for init
    sequencer_->processForward(ppq, sampleOffset);
    
    // send feedback events to LaunchPad
    sequencer_->sendFeedbackEventsToHost(this);
    
    // TODO send MIDI notes to receiver
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

VstInt32 LaunchPlaySequencer::processEvents(VstEvents *events) 
{
    assert(sequencer_.get() != NULL);
    
    sequencer_->processUserEvents(events);
    return 0;
}

void LaunchPlaySequencer::open()
{
    sequencer_.reset(new LaunchPadSequencer);
    sequencer_->init();
}