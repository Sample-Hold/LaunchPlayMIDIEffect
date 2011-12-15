//
//  LaunchPlaySequencer.h
//  LaunchPlayVST
//
//  Created by Fred G on 07/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef LaunchPlayVST_LaunchPlaySequencer_h
#define LaunchPlayVST_LaunchPlaySequencer_h

#include "LaunchPlay.h"
#include "MIDIHelpers.h"

#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include <boost/utility.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

#define kStrideHalf					2
#define kStrideQuarter				1
#define kStrideEight				.5
#define kStrideSixteenth			.25

#define kLaunchPadWidth     		8
#define kLaunchPadHeight    		8
#define kLaunchPadMaxChannel		9
#define kInstrChannelOffset			1

#define kDefaultTempo				120

namespace LaunchPlayVST {

	enum Routing { midi, virtualCable };

    class SequencerBase : boost::noncopyable {
    protected:
        virtual size_t midiEventsCount() const = 0;
        virtual size_t feedbackEventsCount() const = 0;
        virtual void flushMidiEvents(VstEventsBlock *buffer) = 0;
        virtual void flushFeedbackEvents(VstEventsBlock *buffer) = 0;
    public:
        virtual ~SequencerBase() = 0;
        virtual void init() = 0;
        virtual void processForward(double tempo, double ppq, double sampleRate, VstInt32 sampleOffset) = 0;
        virtual void processUserEvents(VstEvents *events) = 0;
        virtual void setBaseNote(VstInt32 note) = 0;
        virtual void setScale(MIDIHelper::Scale scale) = 0;
		virtual void setOctave(VstInt32 channelOffset, VstInt32 octave) = 0;
        virtual VstInt32 getBaseNote() const = 0;
		virtual VstInt32 getOctave(VstInt32 channelOffset) const = 0;
        virtual MIDIHelper::Scale  getScale() const = 0;

		void sendFeedbackEventsToHost(LaunchPlayBase *plugin, VstEventsBlock *buffer);
        void sendMidiEventsToHost(LaunchPlayBase *plugin, VstEventsBlock *buffer, Routing routing = midi);
    };
    
    enum GridDirection { up, down, left, right };
    
    struct Worker { 
        VstInt32 uniqueID, channelOffset;
        size_t x, y;
        GridDirection direction;

		Worker();
		bool operator==(Worker const& other) const;
    };
    
    typedef std::vector<Worker> WorkerList;
    typedef WorkerList::iterator WorkerListIter;
    typedef std::vector<VstMidiEventPtr> VstMidiEventList;
	typedef std::vector<VstDelayedMidiEventPtr> VstDelayedMidiEventList;
	typedef VstDelayedMidiEventList::iterator VstDelayedMidiEventListIter;
    
    class GridSequencer : public SequencerBase {
        struct EqualLocations : std::binary_function<Worker, Worker, bool> {
            bool operator()(Worker const& a, Worker const& b) const;
        };

		struct EqualChannel : std::binary_function<Worker, VstInt32, bool> {
			bool operator()(Worker const& worker, VstInt32 channelOffset) const;
		};

        struct MoveForward : std::binary_function<Worker, Worker, void> {
            void operator()(Worker &worker, Worker const& boundaries) const;
        };
        
        struct HandleCollisions : std::binary_function<Worker, WorkerList, void> {
            void operator()(Worker &worker, WorkerList const& workerList) const;
        };

		struct TestDelayedMidiEvents : std::binary_function<VstDelayedMidiEventPtr, double, bool> {
            bool operator()(VstDelayedMidiEventPtr const midiEvent, double ppq) const;
        };
        
		void generateNotes(double ppq, VstInt32 sampleOffset);

        WorkerList workers_;
        VstMidiEventList midiEvents_;
		VstDelayedMidiEventList delayedMidiEvents_;
        Worker boundaries_;
        VstInt32 baseNote_;
		boost::scoped_array<VstInt32> octaves_;
        MIDIHelper::Scale scale_;
    protected:
        size_t midiEventsCount() const;
        void flushMidiEvents(VstEventsBlock *buffer);
        
        size_t countWorkersAtLocation(Worker worker);
        bool addWorker(Worker const& worker);
        bool removeWorkers(Worker const& worker);
        bool removeAllWorkers(VstInt32 channelOffset);
        WorkerListIter getWorkersBeginIterator();
        WorkerListIter getWorkersEndIterator();
    public:
        GridSequencer(size_t width, size_t height);
        virtual ~GridSequencer();
        
        virtual void processForward(double tempo, double ppq, double sampleRate, VstInt32 sampleOffset);
        
        void setBaseNote(VstInt32 note);
		void setOctave(VstInt32 channelOffset, VstInt32 octave);
        void setScale(MIDIHelper::Scale scale);
        VstInt32 getBaseNote() const;
		VstInt32 getOctave(VstInt32 channelOffset) const;
        MIDIHelper::Scale getScale() const;
    };
    
    enum EditMode { addWorkersMode, removeWorkersMode };
    
    class LaunchPadSequencer : public GridSequencer {
        VstMidiEventList eventsBuffer_;
        GridDirection currentDirection_;
		VstInt32 currentChannelOffset_;
        EditMode currentEditMode_;
        bool primaryBufferEnabled_, removeAll_;
        std::auto_ptr<Worker> tempWorker_;
    protected:
        size_t feedbackEventsCount() const;
        void flushFeedbackEvents(VstEventsBlock *buffer);
        void resetLaunchPad(VstInt32 deltaFrames);
        void setXYLayout(VstInt32 deltaFrames);
        void showWorkers(VstInt32 deltaFrames);
        void cleanWorkers(VstInt32 deltaFrames);
        void changeDirection(GridDirection direction, VstInt32 deltaFrames);
		void changeEditMode(EditMode mode, VstInt32 deltaFrames);
		void changeActiveChannel(VstInt32 channelOffset, VstInt32 deltaFrames);

        void showRemoveButton(VstInt32 deltaFrames);
        
		
        void showTick(double ppq, VstInt32 deltaFrames);
        void swapBuffers(VstInt32 deltaFrames);
    public:
        LaunchPadSequencer();
        ~LaunchPadSequencer();
    
        void init();
        void processUserEvents(VstEvents *eventPtr);
        void processForward(double tempo, double ppq, double sampleRate, VstInt32 sampleOffset);
    };

    class LaunchPlaySequencer : public LaunchPlayBase {
		double currentTempo_, currentBeatsPerSample_;
		Routing currentRouting_;
        boost::shared_ptr<SequencerBase> sequencer_;
		boost::shared_ptr<VstEventsBlock> buffer_;
    protected: 
        void detectTicks(VstTimeInfo *timeInfo, 
                         VstInt32 sampleFrames, 
                         double stride);
        void onTick(double tempo, double ppq, double sampleRate, VstInt32 sampleOffset);
    public:
        LaunchPlaySequencer(audioMasterCallback audioMaster);
        ~LaunchPlaySequencer();
        
		VstPlugCategory getPlugCategory() { return kPlugCategSynth; }
        bool getEffectName(char *name);
        VstInt32 canDo(char *text);
	    
        float getParameter(VstInt32 index);
        void setParameter(VstInt32 index, float value);
        void setParameterAutomated(VstInt32 index, float value);
        void getParameterName(VstInt32 index, char *text);
        void getParameterDisplay(VstInt32 index, char *text);

		VstInt32 getNumMidiInputChannels();
		VstInt32 getNumMidiOutputChannels();

		VstInt32 getChunk(void **data, bool isPreset = false);	
		VstInt32 setChunk(void *data, VstInt32 byteSize, bool isPreset=false);
        
        void open();
        void close();
        VstInt32 processEvents(VstEvents *events);
        void processReplacing (float **inputs, float **outputs, VstInt32 sampleFrames);
    };
    
} // } namespace LaunchPlayVST

#endif