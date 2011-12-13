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

	if(midiEventsCount() == 0)
		return;

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

	if(feedbackEventsCount() == 0)
		return;

    VstEventsBlock eventsBlock;
    eventsBlock.allocate(feedbackEventsCount());
    flushFeedbackEvents(eventsBlock);
    
    VstEvents *events = (VstEvents*) &eventsBlock;
    plugin->sendVstEventsToHost(events);
    
    eventsBlock.deallocate();
}

#pragma mark Worker / GridSequencer
Worker::Worker() : uniqueID(VstInt32(time(NULL)))
{

}

GridSequencer::GridSequencer(size_t width, size_t height)
	: scale_(MIDIHelper::logScaleMajor), baseNote_(0), octaves_(new VstInt32[MIDIHelper::kMaxChannels])
{
    boundaries_.x = width;
    boundaries_.y = height;

	for(VstInt32 i = 0; i < MIDIHelper::kMaxChannels; ++i)
		octaves_[i] = 0;
}

GridSequencer::~GridSequencer()
{

}

bool GridSequencer::EqualLocations::operator()(Worker const& a, Worker const& b) const 
{
    return a.uniqueID != b.uniqueID && a.channel == b.channel && a.x == b.x && a.y == b.y;
}

bool GridSequencer::EqualChannel::operator()(Worker const& worker, VstInt32 channel) const 
{
	return channel == -1 || worker.channel == channel;
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

void GridSequencer::HandleCollisions::operator()(Worker &worker, WorkerList const& workerList) const
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

bool GridSequencer::TestDelayedMidiEvents::operator()(VstDelayedMidiEvent const& delayedMidiEvent, double ppq) const
{
	double diff = ppq - delayedMidiEvent.ppqStart;
	return (diff >= kStrideEight); // midi events are delayed each 1/8th notes
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

bool GridSequencer::removeAllWorkers(VstInt32 channel) 
{
    if(workers_.empty())
        return false;

	bool workersWereRemoved = false;
    WorkerListIter it;
    while((it = std::find_if(workers_.begin(),
							 workers_.end(),
							 std::bind2nd(EqualChannel(), channel))) != workers_.end())
    {
        workersWereRemoved = true;
        workers_.erase(it);
    }
    
    return workersWereRemoved;
}

size_t GridSequencer::countWorkersAtLocation(Worker const& worker)
{
    size_t count = std::count_if(workers_.begin(), 
                                 workers_.end(), 
                                 std::bind2nd(EqualLocations(), worker));
     
    return count;
}

void GridSequencer::processForward(double tempo, double ppq, double sampleRate, VstInt32 sampleOffset)
{
    // move all workers
    for_each(workers_.begin(), workers_.end(), std::bind2nd(MoveForward(), boundaries_));
    
    // detect collisions
    for_each(workers_.begin(), workers_.end(), std::bind2nd(HandleCollisions(), workers_)); 
    
    // generate notes
	generateNotes(ppq, sampleOffset);
}

void GridSequencer::generateNotes(double ppq, VstInt32 sampleOffset)
{
    for(WorkerListIter it = workers_.begin(); it != workers_.end(); ++it) 
	{
		// notes are generated by workers at the topmost location and workers being in collision
        if(it->y == 0 ||
		   std::count_if(workers_.begin(), workers_.end(), std::bind2nd(EqualLocations(), *it)) > 0)
		{
			midiEvents_.push_back(MIDIHelper::createNoteOn(baseNote_, octaves_[it->channel], scale_, VstInt32(it->x), it->channel, sampleOffset));
			delayedMidiEvents_.push_back(VstDelayedMidiEvent(MIDIHelper::createNoteOff(baseNote_, octaves_[it->channel], scale_, VstInt32(it->x), it->channel, sampleOffset), ppq));
		}
	}

	// delayed events are put back in midiEvents_, in order to delay notes off events
	VstDelayedMidiEventListIter it;
    while((it = std::find_if(delayedMidiEvents_.begin(), 
							 delayedMidiEvents_.end(), 
							 std::bind2nd(TestDelayedMidiEvents(), ppq))) != delayedMidiEvents_.end())
    {
		midiEvents_.push_back(it->toMidiEvent(sampleOffset));
		delayedMidiEvents_.erase(it);
    }
}

size_t GridSequencer::midiEventsCount() const
{
    return midiEvents_.size();
}

void GridSequencer::flushMidiEvents(VstEventsBlock &events)
{
    for (size_t i = 0; i < midiEvents_.size(); ++i)        
        VstEventsBlock::convertMidiEvent(midiEvents_[i], events.events[i]);
    
    midiEvents_.clear();
}

void GridSequencer::setBaseNote(VstInt32 note)
{
    assert(note >= 0 && note <= MIDIHelper::kBaseNoteOffsetMaxValue);
    baseNote_ = note;
}

void GridSequencer::setOctave(VstInt32 channelOffset, VstInt32 octave) 
{
	assert(channelOffset >= 0 && channelOffset <= MIDIHelper::kChannelOffsetMaxValue && 
			octave >= MIDIHelper::kBaseNoteMinOctave && 
			octave <= MIDIHelper::kBaseNoteMaxOctave);

	octaves_[channelOffset] = octave;
}

void GridSequencer::setScale(MIDIHelper::Scale scale)
{
    scale_ = scale;
}

VstInt32 GridSequencer::getBaseNote() const
{
    return baseNote_;
}

VstInt32 GridSequencer::getOctave(VstInt32 channel) const 
{
	return octaves_[channel];
}

MIDIHelper::Scale GridSequencer::getScale() const 
{
    return scale_;
}

#pragma mark LaunchPadSequencer
LaunchPadSequencer::LaunchPadSequencer()
: GridSequencer(kLaunchPadWidth, kLaunchPadHeight), 
currentDirection_(up), 
currentChannelOffset_(kLaunchPadChannelOffset), 
currentEditMode_(addWorkersMode), 
tempWorker_(NULL),
primaryBufferEnabled_(false),
removeAll_(false)
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
		if(it->channel != currentChannelOffset_)
			continue;
		
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
    bool tickTack = VstInt32(floor(ppq)) % kStrideHalf == 0;
    
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

void LaunchPadSequencer::showActiveChannel(VstInt32 deltaFrames)
{
	for (VstInt32 i = 0; i < 8; ++i)
	{
		bool activeChannel  = (i+1 == currentChannelOffset_);
        eventsBuffer_.push_back(LaunchPadHelper::createRightButtonMessage(i, 
                                                                        activeChannel ?
                                                                        LaunchPadHelper::fullGreen : 
                                                                        LaunchPadHelper::off,
                                                                        deltaFrames));
	}
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
	setXYLayout(0);
}

void LaunchPadSequencer::processForward(double tempo, double ppq, double sampleRate, VstInt32 sampleOffset) 
{
    // turn workers off
    cleanWorkers(sampleOffset);
    
    // check for "remove all" request
    if(removeAll_) {
		removeAllWorkers(currentChannelOffset_);
        removeAll_ = false;
    }
    
    // check for "remove worker" request
    if(tempWorker_.get() != NULL && currentEditMode_ == removeWorkersMode) {
        removeWorkers(*tempWorker_);
        tempWorker_.release();
    }
    
    // process forward
    GridSequencer::processForward(tempo, ppq, sampleRate, sampleOffset);
    
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
	showActiveChannel(sampleOffset);
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

			// request to change channel
			if(userInput.isRightButton && userInput.btnPressed && userInput.btnNumber < 8) {
				currentChannelOffset_ = userInput.btnNumber + 1;
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
                tempWorker_->x = userInput.x;
                tempWorker_->y = userInput.y;
                tempWorker_->direction = currentDirection_;
				tempWorker_->channel = currentChannelOffset_;
                continue;
            }
        }
    }
}

#pragma mark LaunchPlaySequencer
LaunchPlaySequencer::LaunchPlaySequencer(audioMasterCallback audioMaster)
: LaunchPlayBase(audioMaster, 0, 10), sequencer_(new LaunchPadSequencer)
{ 
    setUniqueID(kSeqUniqueID);
    noTail();
	isSynth();
}

LaunchPlaySequencer::~LaunchPlaySequencer()
{

}

void LaunchPlaySequencer::open()
{
    sequencer_->init();
}

void LaunchPlaySequencer::close()
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
            return normalizeValue(sequencer_->getBaseNote(), MIDIHelper::kBaseNoteOffsetMaxValue);
        case 1: // scale
			return normalizeValue(sequencer_->getScale(), MIDIHelper::kScaleMaxValue);
    }

	if(index > 1 && index < 10) { // octave for each channel
		VstInt32 channelOffset = index - kLaunchPadChannelOffset;
		VstInt32 maxValue = abs(MIDIHelper::kBaseNoteMinOctave) + abs(MIDIHelper::kBaseNoteMaxOctave);
		VstInt32 normalizedOctave = sequencer_->getOctave(channelOffset) +  abs(MIDIHelper::kBaseNoteMinOctave);

		return normalizeValue(normalizedOctave, maxValue);
	}
    
    return 0;
}

void LaunchPlaySequencer::setParameter(VstInt32 index, float value)
{
    switch (index) {
        case 0: // base note
			sequencer_->setBaseNote(denormalizeValue(value, MIDIHelper::kBaseNoteOffsetMaxValue));
			break;
        case 1: // scale
			sequencer_->setScale(MIDIHelper::Scale(denormalizeValue(value, MIDIHelper::kScaleMaxValue)));
			break;
    }

	if(index > 1 && index < 10) { // octave for each channel
		VstInt32 channelOffset = index - kLaunchPadChannelOffset;
		VstInt32 maxValue = abs(MIDIHelper::kBaseNoteMinOctave) + abs(MIDIHelper::kBaseNoteMaxOctave);
		VstInt32 normalizedOctave = denormalizeValue(value, maxValue);
		VstInt32 octave = normalizedOctave - abs(MIDIHelper::kBaseNoteMinOctave);

		sequencer_->setOctave(channelOffset, octave);
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
					vst_strncpy(text, "MinorNat", kVstMaxParamStrLen);
					break;
				case 2:
					vst_strncpy(text, "MinorHar", kVstMaxParamStrLen);
					break;
				case 3:
					vst_strncpy(text, "MinMelAsc", kVstMaxParamStrLen);
					break;
				case 4:
					vst_strncpy(text, "MinMelDesc", kVstMaxParamStrLen);
					break;
				case 5:
					vst_strncpy(text, "Pent Major", kVstMaxParamStrLen);
					break;
				case 6:
					vst_strncpy(text, "Pent Minor Nat", kVstMaxParamStrLen);
					break;
				case 7:
					vst_strncpy(text, "Whole Note", kVstMaxParamStrLen);
					break;
				case 8:
					vst_strncpy(text, "Augmented", kVstMaxParamStrLen);
					break;
				case 9:
					vst_strncpy(text, "Prometheus", kVstMaxParamStrLen);
					break;
				case 10:
					vst_strncpy(text, "Blues", kVstMaxParamStrLen);
					break;
				case 11:
					vst_strncpy(text, "Tritone", kVstMaxParamStrLen);
					break;
            }
            break;
    }

	if(index > 1 && index < 10) { // octave for each channel
		VstInt32 channelOffset = index - kLaunchPadChannelOffset;
		std::stringstream ss;
		ss << sequencer_->getOctave(channelOffset);
		vst_strncpy(text, ss.str().c_str(), kVstMaxParamStrLen);
		return;
	}
}

void LaunchPlaySequencer::getParameterName(VstInt32 index, char *text)
{
    switch (index) {
        case 0:
            vst_strncpy(text, "Note", kVstMaxParamStrLen);
            break;
        case 1:
            vst_strncpy(text, "Scale", kVstMaxParamStrLen);
            break;
	}

	if(index > 1 && index < 10) { // octave for each channel
		std::stringstream ss;
		ss << "Octave ";
		ss << (index-1);

        vst_strncpy(text, ss.str().c_str(), kVstMaxParamStrLen);
		return;
    }
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
        onTick(timeInfo->tempo, ppqPos, timeInfo->sampleRate, VstInt32((ppqPos - startPpqPos) / beatsPerSample));
}

void LaunchPlaySequencer::onTick(double tempo, double ppq, double sampleRate, VstInt32 sampleOffset)
{
    assert(sequencer_.get() != NULL && sampleOffset >= 0);

    // move sequencer forward
    sequencer_->processForward(tempo, ppq, sampleRate, sampleOffset);
    
    // send feedback events to Host
    sequencer_->sendFeedbackEventsToHost(this);
    
    // send generated notes to Host
	sequencer_->sendMidiEventsToHost(this);
}