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
using namespace boost::interprocess;

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

void SequencerBase::sendMIDIEventsToEmitter(message_queue *mq)
{
    assert(mq != NULL);
    
    VstEventsBlock eventsBlock;
    eventsBlock.allocate(midiEventsCount());
    flushMidiEvents(eventsBlock);
    
    mq->send(&eventsBlock, sizeof(eventsBlock), 0);
    
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
: scale_(MIDIHelper::majorScale), baseNote_(0)
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

void GridSequencer::processForward(double tempo, double ppq, double beatsPerSample, VstInt32 sampleOffset)
{
    // move all workers
    for_each(workers_.begin(), workers_.end(), std::bind2nd(MoveForward(), boundaries_));
    
    // detect collisions
    for_each(workers_.begin(), workers_.end(), std::bind2nd(HandleCollisions(), workers_)); 
    
    // generate notes
    generateNotes(beatsPerSample, sampleOffset);
}

void GridSequencer::generateNotes(double beatsPerSample, VstInt32 sampleOffset)
{
    for(WorkerListIter it = workers_.begin(); it != workers_.end(); ++it) {
        if(!it->y == 0)
            continue;
        
        notes_.push_back(MIDIHelper::createNoteOn(baseNote_, scale_, VstInt32(it->x), sampleOffset));
        //notes_.push_back(MIDIHelper::createNoteOff(baseNote_, scale_, VstInt32(it->x), sampleOffset + .05 / beatsPerSample));
    }
}

size_t GridSequencer::midiEventsCount() const
{
    return notes_.size();
}

void GridSequencer::flushMidiEvents(VstEventsBlock &events)
{
    for (size_t i = 0; i < notes_.size(); ++i)        
        VstEventsBlock::convertMidiEvent(notes_[i], events.events[i]);
    
    notes_.clear();
}

void GridSequencer::setBaseNote(VstInt32 note)
{
    assert(note >= 0 && note < 12);
    baseNote_ = note;
}

void GridSequencer::setScale(MIDIHelper::Scale scale)
{
    scale_ = scale;
}

VstInt32 GridSequencer::getBaseNote() const
{
    return baseNote_;
}

MIDIHelper::Scale GridSequencer::getScale() const 
{
    return scale_;
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
    eventsBuffer_.push_back(LaunchPadHelper::createSetLayoutMessage(LaunchPadHelper::xYLayout,
                                                                    deltaFrames));
}

void LaunchPadSequencer::cleanWorkers(VstInt32 deltaFrames)
{
    for(WorkerListIter it = getWorkersBeginIterator(); it != getWorkersEndIterator(); ++it) {
        eventsBuffer_.push_back(LaunchPadHelper::createGridButtonMessage(it->x, 
                                                                         it->y,
                                                                         LaunchPadHelper::off,
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
                                                                         LaunchPadHelper::fullRed : 
                                                                         LaunchPadHelper::fullGreen,
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
                                                                        LaunchPadHelper::fullAmber : 
                                                                        LaunchPadHelper::off,
                                                                        deltaFrames));
    }
}

void LaunchPadSequencer::showEditMode(VstInt32 deltaFrames)
{
    eventsBuffer_.push_back(LaunchPadHelper::createTopButtonMessage(4, 
                                                                    currentEditMode_ == addWorkersMode ?
                                                                    LaunchPadHelper::fullAmber : 
                                                                    LaunchPadHelper::fullRed,
                                                                    deltaFrames));
}

void LaunchPadSequencer::showRemoveButton(VstInt32 deltaFrames)
{
    eventsBuffer_.push_back(LaunchPadHelper::createTopButtonMessage(5, 
                                                                    LaunchPadHelper::fullAmber,
                                                                    deltaFrames));
}

void LaunchPadSequencer::showTick(double ppq, VstInt32 deltaFrames)
{
    bool tickTack = VstInt32(floor(ppq)) % 2 == 0;
    
    eventsBuffer_.push_back(LaunchPadHelper::createTopButtonMessage(6, 
                                                                    tickTack? 
                                                                    LaunchPadHelper::fullRed : 
                                                                    LaunchPadHelper::off,
                                                                    deltaFrames));
    eventsBuffer_.push_back(LaunchPadHelper::createTopButtonMessage(7, 
                                                                    tickTack? 
                                                                    LaunchPadHelper::off : 
                                                                    LaunchPadHelper::fullRed,
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

void LaunchPadSequencer::processForward(double tempo, double ppq, double beatsPerSample, VstInt32 sampleOffset) 
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
    
    // process forward
    GridSequencer::processForward(tempo, ppq, beatsPerSample, sampleOffset);
    
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
            LaunchPadUserInput userInput = LaunchPadHelper::readMessage(midiEvent, LaunchPadHelper::xYLayout);

            // request to change direction
            if(userInput.isTopButton && userInput.btnPressed && userInput.btnNumber < 4) {
                currentDirection_ = (GridDirection) userInput.btnNumber;
                continue;
            }
            
            // request to change edit mode
            if(userInput.isTopButton && userInput.btnPressed && userInput.btnNumber == 4) {
                currentEditMode_ = (currentEditMode_ == addWorkersMode) ? 
                removeWorkersMode : addWorkersMode; 
                continue;
            }
            
            // request to remove all workers
            if(userInput.isTopButton && userInput.btnPressed && userInput.btnNumber == 5) {
                removeAll_ = true;
                continue;
            }
            
            // request to remove a worker 
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
: LaunchPlayBase(audioMaster, 0, 2)
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

float LaunchPlaySequencer::getParameter(VstInt32 index)
{
    switch (index) {
        case 0: // base note
            return float(sequencer_->getBaseNote()) / MIDIHelper::kBaseNoteMaxValue;
        case 1: // scale
            return float(sequencer_->getScale()) / MIDIHelper::kScaleMaxValue;
    }
    
    return 0;
}

void LaunchPlaySequencer::setParameter(VstInt32 index, float value)
{
    switch (index) {
        case 0: // base note
            sequencer_->setBaseNote(VstInt32(floor((value * (float) MIDIHelper::kBaseNoteMaxValue) + .5)));
        case 1: // scale
            sequencer_->setScale(MIDIHelper::Scale((uint32_t) floor((value * (float) MIDIHelper::kScaleMaxValue) + .5)));
    }
}

void LaunchPlaySequencer::setParameterAutomated(VstInt32 index, float value)
{
    setParameter(index, value);
}

void LaunchPlaySequencer::getParameterDisplay(VstInt32 index, char *text)
{
    switch (index) {
        case 0: // base note
            switch (sequencer_->getBaseNote()) {
                case 0:
                    vst_strncpy(text, "C", kVstMaxParamStrLen);
                    break;
                case 1:
                    vst_strncpy(text, "C#", kVstMaxParamStrLen);
                    break;
                case 2:
                    vst_strncpy(text, "D", kVstMaxParamStrLen);
                    break;
                case 3:
                    vst_strncpy(text, "D#", kVstMaxParamStrLen);
                    break;
                case 4:
                    vst_strncpy(text, "E", kVstMaxParamStrLen);
                    break;
                case 5:
                    vst_strncpy(text, "F", kVstMaxParamStrLen);
                    break;
                case 6:
                    vst_strncpy(text, "F#", kVstMaxParamStrLen);
                    break;
                case 7:
                    vst_strncpy(text, "G", kVstMaxParamStrLen);
                    break;
                case 8:
                    vst_strncpy(text, "G#", kVstMaxParamStrLen);
                    break;
                case 9:
                    vst_strncpy(text, "A", kVstMaxParamStrLen);
                    break;
                case 10:
                    vst_strncpy(text, "A#", kVstMaxParamStrLen);
                    break;
                case 11:
                    vst_strncpy(text, "B", kVstMaxParamStrLen);
                    break;
            }
            break;
        case 1: // scale
            switch (sequencer_->getScale()) {
                case 0:
                    vst_strncpy(text, "Major", kVstMaxParamStrLen);
                    break;
                case 1:
                    vst_strncpy(text, "Minor", kVstMaxParamStrLen);
                    break;
            }
            break;
    }
}

void LaunchPlaySequencer::getParameterName(VstInt32 index, char *text)
{
    switch (index) {
        case 0:
            vst_strncpy(text, "Base Note", kVstMaxParamStrLen);
            break;
        case 1:
            vst_strncpy(text, "Scale", kVstMaxParamStrLen);
            break;
    }
}

bool LaunchPlaySequencer::getParameterProperties(VstInt32 index, VstParameterProperties *p)
{
    switch (index) {
        case 0: // base note
            p->displayIndex = 0;
            vst_strncpy(p->label, "Base Note", kVstMaxParamStrLen);
            p->flags = kVstParameterSupportsDisplayIndex | kVstParameterUsesIntegerMinMax | kVstParameterCanRamp;
            p->minInteger = 0;
            p->maxInteger = 11;
            p->stepInteger = 1;
            return true;
        case 1: // scale
            p->displayIndex = 1;
            vst_strncpy(p->label, "Scale", kVstMaxParamStrLen);
            p->flags = kVstParameterSupportsDisplayIndex | kVstParameterUsesIntegerMinMax | kVstParameterCanRamp;
            p->minInteger = 0;
            p->maxInteger = 11;
            p->stepInteger = 1;
            return true;
    }
    
    return false;
}

void LaunchPlaySequencer::open()
{
    sequencer_.reset(new LaunchPadSequencer);
    sequencer_->init();
    
    mq_.reset(new message_queue(open_or_create, kMessageQueueName, 2, sizeof(VstEventsBlock)));
}

void LaunchPlaySequencer::close()
{
    sequencer_.release();
    
    mq_->remove(kMessageQueueName);
    mq_.release();
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
        onTick(timeInfo->tempo, ppqPos, beatsPerSample, VstInt32((ppqPos - startPpqPos) / beatsPerSample));
}

void LaunchPlaySequencer::onTick(double tempo, double ppq, double beatsPerSample, VstInt32 sampleOffset)
{
    assert(sequencer_.get() != NULL && sampleOffset >= 0);
    
    // trick to initialize LaunchPad on first frame
    if(sampleOffset == 0)
        sequencer_->sendFeedbackEventsToHost(this);
    
    // move sequencer forward
    sequencer_->processForward(tempo, ppq, beatsPerSample, sampleOffset);
    
    // send feedback events to LaunchPad
    sequencer_->sendFeedbackEventsToHost(this);
    
    // send generated notes to LaunchPlayEmitter
    sequencer_->sendMIDIEventsToEmitter(mq_.get());
}