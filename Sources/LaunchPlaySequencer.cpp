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
#include <algorithm>
#include <functional>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/thread.hpp>

using namespace LaunchPlayVST;

#pragma mark createEffectInstance
AudioEffect* createEffectInstance (audioMasterCallback audioMaster)
{
    return new LaunchPlaySequencer(audioMaster); 
}

#pragma mark SequencerBase
SequencerBase::~SequencerBase() // to avoid linker error
{
    
}

void SequencerBase::sendEventsUsingMIDI(LaunchPlayBase *plugin,  
                                        size_t eventsCount, 
                                        FlushFunction flush)
{
    VstEventsBlock buffer;
    
    try {
        buffer.allocate(eventsCount);
        (this->*flush)(buffer);
        plugin->sendVstEventsToHost((VstEvents*) &buffer);
    }
    catch(...) { buffer.deallocate(); return; }
    
    buffer.deallocate();
}

void SequencerBase::sendEventsUsingVirtualCable(size_t eventsCount, 
                                                FlushFunction flush,
                                                bool async)
{
    VstEventsBlock buffer;
    
    try {
        buffer.allocate(eventsCount);
        (this->*flush)(buffer);

        for(VstInt32 channelOffset = 0; channelOffset <= kMaxMIDIChannelOffset; ++channelOffset) {
            VstEventsBlock filteredEventsBlock = buffer.getFilteredMidiEvents(channelOffset);
            
            if(filteredEventsBlock.numEvents == 0)
                continue;

            std::stringstream ss;
            boost::archive::binary_oarchive archive(ss);
            archive << filteredEventsBlock;
                
            if(async)
            {
                boost::thread t(&SequencerBase::sendMessage, this, ss.str(), channelOffset);
                t.detach();
            }
            else
                sendMessage(ss.str(), channelOffset);
        }
    }
    catch(...) { buffer.deallocate(); return; }
    
    buffer.deallocate();
}

void SequencerBase::sendMessage(std::string const message, VstInt32 const channelOffset) const
{
    using namespace boost::interprocess;
    
    try {
        std::stringstream ss;
        ss << kMessageQueueNames;
        ss << channelOffset;
        
        message_queue mq(open_only, ss.str().c_str());
        mq.try_send(message.c_str(), message.length(), 0);
    }
    catch(interprocess_exception) {
        //printf("Cannot communicate with virtual cable %d\n", channelOffset);
    }
}

void SequencerBase::sendEventsToHost(LaunchPlayBase *plugin, Routing routing)
{
	assert(midiEventsCount() <= VstEventsBlock::kVstEventsBlockSize && 
           feedbackEventsCount() <= VstEventsBlock::kVstEventsBlockSize);
    
	bool asyncImpl = false; // not sure I should enable it...

    switch (routing) {
        case midi:
            if(midiEventsCount() > 0)
                sendEventsUsingMIDI(plugin, midiEventsCount(), &SequencerBase::flushMidiEvents);
            
            if(feedbackEventsCount() > 0)
                sendEventsUsingMIDI(plugin, feedbackEventsCount(), &SequencerBase::flushFeedbackEvents);

            break;
        case virtualCable:
            if(midiEventsCount() > 0)
                sendEventsUsingVirtualCable(midiEventsCount(), &SequencerBase::flushMidiEvents, asyncImpl);
            
            if(feedbackEventsCount() > 0)
                sendEventsUsingVirtualCable(feedbackEventsCount(), &SequencerBase::flushFeedbackEvents, asyncImpl);
            
            // trick for some hosts that stop sending timeinfo after 1 mesure if no midi event has been sent
            VstEvents dummyEvent;
            VstMidiEventPtr dummyMidi = MIDIHelper::createDummy();
            
            dummyEvent.numEvents = 1;
            dummyEvent.events[0] = (VstEvent*) dummyMidi.get();
            plugin->sendVstEventsToHost(&dummyEvent);
            
            break;
    }
}

#pragma mark Worker
Worker::Worker() : uniqueID(VstInt32(time(NULL)))
{

}

bool Worker::operator==(Worker const& other) const
{
	return other.uniqueID == uniqueID;
}

#pragma mark GridSequencer
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
    return a.uniqueID != b.uniqueID && a.channelOffset == b.channelOffset && a.x == b.x && a.y == b.y;
}

bool GridSequencer::EqualChannel::operator()(Worker const& worker, VstInt32 channelOffset) const 
{
	return worker.channelOffset == channelOffset;
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

bool GridSequencer::TestDelayedMidiEvents::operator()(VstDelayedMidiEventPtr const midiEvent, double ppq) const
{
	double diff = ppq - midiEvent->ppqStart;
	return (diff >= kStrideQuarter); // midi events are delayed each 1/4th notes
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
	if(worker.x >= boundaries_.x || worker.y >= boundaries_.y)
		return false;

    if(workers_.empty() || countWorkersAtLocation(worker) == 0)
    {
        workers_.push_back(worker);
        return true;
    }
    
    return false;
}

bool GridSequencer::removeWorkers(Worker const& worker)
{
    if(worker.x >= boundaries_.x || worker.y >= boundaries_.y)
		return false;

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

bool GridSequencer::removeAllWorkers(VstInt32 channelOffset) 
{
    if(workers_.empty())
        return false;

	bool workersWereRemoved = false;
    WorkerListIter it;
    while((it = std::find_if(workers_.begin(),
							 workers_.end(),
							 std::bind2nd(EqualChannel(), channelOffset))) != workers_.end())
    {
        workersWereRemoved = true;
        workers_.erase(it);
    }
    
    return workersWereRemoved;
}

size_t GridSequencer::countWorkersAtLocation(Worker worker)
{
	worker.uniqueID = -1; // trick to count all worker, including "worker"

    size_t count = std::count_if(workers_.begin(), 
                                 workers_.end(), 
                                 std::bind2nd(EqualLocations(), worker));
     
    return count;
}

void GridSequencer::processForward(double tempo, double ppq, double sampleRate, VstInt32 sampleOffset)
{
    // move all workers
    for_each(workers_.begin(), workers_.end(), std::bind2nd(MoveForward(), boundaries_));
    
	// handle collisions
    for_each(workers_.begin(), workers_.end(), std::bind2nd(HandleCollisions(), workers_)); 

	// generate notes on
	generateNotes(ppq, sampleOffset);
}

void GridSequencer::generateNotes(double ppq, VstInt32 sampleOffset)
{
	std::vector<Worker> tempArray;

    for(WorkerListIter it = workers_.begin(); it != workers_.end(); ++it) 
	{
		// notes are generated by workers at the topmost location and by workers in collision
        if(it->y != 0 && countWorkersAtLocation(*it) < 2)
			continue;

		// workers at same location generate notes once
		if(std::find_if(tempArray.begin(), tempArray.end(), std::bind2nd(EqualLocations(), *it)) != tempArray.end())
			continue;

		tempArray.push_back(*it);

		char noteChannel = char(kInstrChannelOffset) + it->channelOffset;

		midiEvents_.push_back(MIDIHelper::createNoteOn(baseNote_, 
													   octaves_[noteChannel], 
													   scale_, 
													   VstInt32(it->x), 
													   sampleOffset, 
													   noteChannel));
		delayedMidiEvents_.push_back(VstDelayedMidiEventPtr(new VstDelayedMidiEvent(
														MIDIHelper::createNoteOff(baseNote_, 
																				  octaves_[noteChannel], 
																				  scale_, 
																				  VstInt32(it->x), 
																				  sampleOffset, 
																				  noteChannel), ppq)));
	}

	// delayed events are put back in midiEvents_ in order to delay notes off events
	VstDelayedMidiEventListIter it;
    while((it = std::find_if(delayedMidiEvents_.begin(), 
							 delayedMidiEvents_.end(), 
							 std::bind2nd(TestDelayedMidiEvents(), ppq))) != delayedMidiEvents_.end())
    {
		midiEvents_.push_back((*it)->toMidiEvent(sampleOffset));
		delayedMidiEvents_.erase(it);
    }
}

size_t GridSequencer::midiEventsCount() const
{
    return midiEvents_.size();
}

void GridSequencer::flushMidiEvents(VstEventsBlock &buffer)
{
    for (size_t i = 0; i < midiEvents_.size(); ++i)        
        VstEventsBlock::convertMidiEvent(midiEvents_[i].get(), buffer.events[i]);
    
    midiEvents_.clear();
}

void GridSequencer::setBaseNote(VstInt32 note)
{
    assert(note >= 0 && note < MIDIHelper::kBaseNoteCount);
    baseNote_ = note;
}

void GridSequencer::setOctave(VstInt32 channelOffset, VstInt32 octave) 
{
	assert(channelOffset >= 0 && channelOffset < MIDIHelper::kMaxChannels && 
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
: GridSequencer(kLaunchPadWidth, kLaunchPadHeight), currentDirection_(up), 
currentChannelOffset_(0), currentEditMode_(addWorkersMode), 
tempWorker_(NULL), primaryBufferEnabled_(false), removeAll_(false)
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
		if(it->channelOffset != currentChannelOffset_)
			continue;
		
		bool collision = countWorkersAtLocation(*it) > 1;

        eventsBuffer_.push_back(LaunchPadHelper::createGridButtonMessage(it->x, 
                                                                         it->y,
                                                                         collision ? 
                                                                         LaunchPadHelper::fullRed : 
                                                                         LaunchPadHelper::fullGreen,
                                                                         deltaFrames));
    }
}

void LaunchPadSequencer::changeDirection(GridDirection direction, VstInt32 deltaFrames) 
{
	if(direction != currentDirection_)
		eventsBuffer_.push_back(LaunchPadHelper::createTopButtonMessage(currentDirection_, 
																		LaunchPadHelper::off,
																		deltaFrames, 
																		LaunchPadHelper::clearOtherBuffer));

	eventsBuffer_.push_back(LaunchPadHelper::createTopButtonMessage(direction, 
                                                                    LaunchPadHelper::fullAmber,
																	deltaFrames, 
																	LaunchPadHelper::copyBothBuffers));
	currentDirection_ = direction;
}

void LaunchPadSequencer::changeEditMode(EditMode mode, VstInt32 deltaFrames)
{
    eventsBuffer_.push_back(LaunchPadHelper::createTopButtonMessage(4, 
                                                                    mode == addWorkersMode ?
                                                                    LaunchPadHelper::fullAmber : 
                                                                    LaunchPadHelper::fullRed,
                                                                    deltaFrames, 
																	LaunchPadHelper::copyBothBuffers));
	currentEditMode_ = mode;
}

void LaunchPadSequencer::changeActiveChannel(VstInt32 channelOffset, VstInt32 deltaFrames)
{
	if(channelOffset != currentChannelOffset_)
		eventsBuffer_.push_back(LaunchPadHelper::createRightButtonMessage(currentChannelOffset_, 
																		  LaunchPadHelper::off,
                                                                          deltaFrames));

	eventsBuffer_.push_back(LaunchPadHelper::createRightButtonMessage(channelOffset, 
																		  LaunchPadHelper::fullGreen,
                                                                          deltaFrames));

	currentChannelOffset_ = channelOffset;
}

void LaunchPadSequencer::showRemoveButton(VstInt32 deltaFrames)
{
    eventsBuffer_.push_back(LaunchPadHelper::createTopButtonMessage(5, 
                                                                    LaunchPadHelper::fullAmber,
                                                                    deltaFrames, 
																	LaunchPadHelper::copyBothBuffers));
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

void LaunchPadSequencer::flushFeedbackEvents(VstEventsBlock &buffer)
{
    for (size_t i = 0; i < eventsBuffer_.size(); ++i)        
        VstEventsBlock::convertMidiEvent(eventsBuffer_[i].get(), buffer.events[i]);

    eventsBuffer_.clear();
}

void LaunchPadSequencer::init()
{
    resetLaunchPad(0);
	changeDirection(currentDirection_, 1);
	changeActiveChannel(currentChannelOffset_, 1);
	changeEditMode(currentEditMode_, 1);
	showRemoveButton(1);
}

void LaunchPadSequencer::processForward(double tempo, double ppq, double sampleRate, VstInt32 sampleOffset) 
{
    // turn workers off
	if(ppq > 0)
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
    
     // show tick
    showTick(ppq, sampleOffset);

    // swap buffers
	swapBuffers(++sampleOffset);
}

void LaunchPadSequencer::processUserEvents(VstEvents *eventPtr)
{
    for (VstInt32 i = 0; i < eventPtr->numEvents; ++i) 
    {
        VstEvent *event = eventPtr->events[i]; 
        
        if(event->type != kVstMidiType)
            continue;
        
        VstMidiEvent *midiEvent = (VstMidiEvent*) event;  
		MIDIMessage const message(midiEvent);

		if(!LaunchPadHelper::isValidMessage(message))
			continue;

        LaunchPadUserInput userInput = LaunchPadHelper::readMessage(message, LaunchPadHelper::xYLayout);

        // request to change direction
        if(userInput.isTopButton && userInput.btnPressed && userInput.btnNumber < 4) {
			changeDirection(GridDirection(userInput.btnNumber), midiEvent->deltaFrames);
            continue;
        }
            
        // request to change edit mode
        if(userInput.isTopButton && userInput.btnPressed && userInput.btnNumber == 4) {
            EditMode newMode((currentEditMode_ == addWorkersMode) ? 
							removeWorkersMode : addWorkersMode); 
			changeEditMode(newMode, midiEvent->deltaFrames);
            continue;
        }

		// request to change channel
		if(userInput.isRightButton && userInput.btnPressed && userInput.btnNumber < 8) {
			changeActiveChannel(userInput.btnNumber, midiEvent->deltaFrames);
			continue;
		}
            
        // request to remove all workers
        if(userInput.isTopButton && userInput.btnPressed && userInput.btnNumber == 5) {
            removeAll_ = true;
            continue;
        }
            
        // request to add or remove a worker 
        if(userInput.isGridButton && userInput.btnReleased) {
            tempWorker_ = std::auto_ptr<Worker>(new Worker);
            tempWorker_->x = userInput.x;
            tempWorker_->y = userInput.y;
            tempWorker_->direction = currentDirection_;
			tempWorker_->channelOffset = currentChannelOffset_; 
            continue;
        }
    }
}

#pragma mark LaunchPlaySequencer
LaunchPlaySequencer::LaunchPlaySequencer(audioMasterCallback audioMaster)
: LaunchPlayBase(audioMaster, 0, 11), sequencer_(new LaunchPadSequencer), currentRouting_(midi)
{ 
    setUniqueID(kSeqUniqueID);
    noTail();
	isSynth();
	programsAreChunks();
}

LaunchPlaySequencer::~LaunchPlaySequencer()
{

}

void LaunchPlaySequencer::open()
{
    sequencer_->init();
}

bool LaunchPlaySequencer::getEffectName(char* name)
{
	vst_strncpy (name, kSequencerName, kVstMaxEffectNameLen);
	return true;
}

VstInt32 LaunchPlaySequencer::canDo(char *text)
{
    if(strcmp(text, "sendVstMidiEvent") == 0 || 
       strcmp(text, "sendVstEvents") == 0 || 
       strcmp(text, "receiveVstEvents") == 0 ||
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
            return normalizeValue(sequencer_->getBaseNote(), MIDIHelper::kBaseNoteCount-1);
        case 1: // scale
			return normalizeValue(sequencer_->getScale(), MIDIHelper::kScaleMaxValue);
		case 10: // routing
			return float(currentRouting_);
    }

	if(index > 1 && index < 10) { // octave for each channel
		VstInt32 maxValue = abs(MIDIHelper::kBaseNoteMinOctave) + abs(MIDIHelper::kBaseNoteMaxOctave);
		VstInt32 normalizedOctave = sequencer_->getOctave(index-1) +  abs(MIDIHelper::kBaseNoteMinOctave);

		return normalizeValue(normalizedOctave, maxValue);
	}
    
    return 0;
}

void LaunchPlaySequencer::setParameter(VstInt32 index, float value)
{
    switch (index) {
        case 0: // base note
			sequencer_->setBaseNote(denormalizeValue(value, MIDIHelper::kBaseNoteCount-1));
			break;
        case 1: // scale
			sequencer_->setScale(MIDIHelper::Scale(denormalizeValue(value, MIDIHelper::kScaleMaxValue)));
			break;
		case 10: // routing
			currentRouting_ = Routing(denormalizeValue(value, 1));  
    }

	if(index > 1 && index < 10) { // octave for each channel
		VstInt32 maxValue = abs(MIDIHelper::kBaseNoteMinOctave) + abs(MIDIHelper::kBaseNoteMaxOctave);
		VstInt32 normalizedOctave = denormalizeValue(value, maxValue);
		VstInt32 octave = normalizedOctave - abs(MIDIHelper::kBaseNoteMinOctave);

		sequencer_->setOctave(index-1, octave);
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
		case 10: // routing
            switch (currentRouting_) {
			case midi:
				vst_strncpy(text, "MIDI", kVstMaxParamStrLen);
				break;
			case virtualCable:
				vst_strncpy(text, "Virtual", kVstMaxParamStrLen);
				break;
			}
    }

	if(index > 1 && index < 10) { // octave for each channel
		std::stringstream ss;
		ss << sequencer_->getOctave(index-1);
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
		case 10:
            vst_strncpy(text, "Routing", kVstMaxParamStrLen);
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

VstInt32 LaunchPlaySequencer::getNumMidiInputChannels()
{
	return 1;
}

VstInt32 LaunchPlaySequencer::getNumMidiOutputChannels()
{
	return kMaxMIDIChannelOffset+1;
}

VstInt32 LaunchPlaySequencer::getChunk(void **data, bool isPreset) {
	*data = &currentRouting_;

	return sizeof(currentRouting_);
}

VstInt32 LaunchPlaySequencer::setChunk(void *data, VstInt32 byteSize, bool isPreset) {
	if(byteSize == sizeof(VstInt32)) {
		memcpy(&currentRouting_, data, byteSize);
	}

	return 0;
}

void LaunchPlaySequencer::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
    VstTimeInfo *time = getTimeInfo(kVstTransportChanged | kVstTransportPlaying | kVstTempoValid | kVstPpqPosValid);
    
    if (time 
        && time->flags & kVstTransportPlaying
        && time->flags & kVstTempoValid
        && time->flags & kVstPpqPosValid) 
    {
        detectTicks(time, sampleFrames, kStrideEight); // 1/8th notes
    }


	// null audio output
	for(VstInt32 i = 0; i < cEffect.numOutputs; ++i)
		memset(outputs[i], 0, sampleFrames*sizeof(float));
}

void LaunchPlaySequencer::onTick(double tempo, double ppq, double sampleRate, VstInt32 sampleOffset)
{
    assert(sequencer_.get() != NULL);

    // move sequencer forward
    sequencer_->processForward(tempo, ppq, sampleRate, sampleOffset);
    
    // send generated notes and feedback events to VST Host
	sequencer_->sendEventsToHost(this, currentRouting_);
}

VstInt32 LaunchPlaySequencer::processEvents(VstEvents *events) 
{
    assert(sequencer_.get() != NULL);
    
    sequencer_->processUserEvents(events);
    return 0;
}