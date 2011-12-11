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

#define kStrideQuarter          1
#define kStrideEight            .5
#define kStrideSixteenth        .25

#define kLaunchPadWidth         8
#define kLaunchPadHeight        8

namespace LaunchPlayVST {

    class SequencerBase {
    protected:
        virtual size_t midiEventsCount() const = 0;
        virtual size_t feedbackEventsCount() const = 0;
        virtual void flushMidiEvents(VstEventsBlock &events) = 0;
        virtual void flushFeedbackEvents(VstEventsBlock &events) = 0;
    public:
        virtual ~SequencerBase() = 0;
        virtual void init() = 0;
        virtual void processForward(double ppq, VstInt32 sampleOffset) = 0;
        virtual void processUserEvents(VstEvents *events) = 0;
        
        void sendMidiEventsToHost(LaunchPlayBase *plugin);
        void sendFeedbackEventsToHost(LaunchPlayBase *plugin);
    };
    
    enum GridDirection { up, down, left, right };
    
    struct Worker { 
        VstInt32 uniqueID;
        size_t x;
        size_t y;
        GridDirection direction;
    };
    
    typedef std::vector<Worker> WorkerList;
    typedef WorkerList::iterator WorkerListIter;
    
    class GridSequencer : public SequencerBase, boost::noncopyable {
        struct EqualLocations : std::binary_function<Worker, Worker, bool> {
            bool operator()(Worker const& a, Worker const& b) const;
        };
        
        struct MoveForward : std::binary_function<Worker, Worker, void> {
            void operator()(Worker & worker, Worker const& boundaries) const;
        };
        
        struct HandleCollisions : std::binary_function<Worker, WorkerList, void> {
            void operator()(Worker & worker, WorkerList const& workerList) const;
        };
        
        WorkerList workers_;
        Worker boundaries_;
    protected:
        virtual size_t midiEventsCount() const;
        virtual void flushMidiEvents(VstEventsBlock &events);
        
        size_t countWorkersAtLocation(Worker const& worker);
        bool addWorker(Worker const& worker);
        bool removeWorkers(Worker const& worker);
        bool removeAllWorkers();
        WorkerListIter getWorkersBeginIterator();
        WorkerListIter getWorkersEndIterator();
    public:
        GridSequencer(size_t width, size_t height);
        virtual ~GridSequencer();
        
        virtual void processForward(double ppq, VstInt32 sampleOffset);
    };
    
    enum EditMode { addWorkersMode, removeWorkersMode };
    
    class LaunchPadSequencer : public GridSequencer {
        std::vector<VstMidiEvent> eventsBuffer_;
        GridDirection currentDirection_;
        EditMode currentEditMode_;
        bool primaryBufferEnabled_, removeAll_;
        std::auto_ptr<Worker> tempWorker_;
    protected:
        virtual size_t feedbackEventsCount() const;
        virtual void flushFeedbackEvents(VstEventsBlock &events);
        
        void resetLaunchPad(VstInt32 deltaFrames);
        void setXYLayout(VstInt32 deltaFrames);
        void showWorkers(VstInt32 deltaFrames);
        void cleanWorkers(VstInt32 deltaFrames);
        void showDirections(VstInt32 deltaFrames);
        void showRemoveButton(VstInt32 deltaFrames);
        void showEditMode(VstInt32 deltaFrames);
        void showTick(double ppq, VstInt32 deltaFrames);
        void swapBuffers(VstInt32 deltaFrames);
    public:
        LaunchPadSequencer();
        virtual ~LaunchPadSequencer();
    
        virtual void init();
        virtual void processUserEvents(VstEvents *eventPtr);
        virtual void processForward(double ppq, VstInt32 sampleOffset);
    };

    class LaunchPlaySequencer : public LaunchPlayBase {
        std::auto_ptr<SequencerBase> sequencer_;
    protected: 
        virtual void detectTicks(VstTimeInfo *timeInfo, 
                                 VstInt32 sampleFrames, 
                                 double stride = kStrideEight);
        virtual void onTick(double ppq, VstInt32 sampleOffset);
    public:
        LaunchPlaySequencer(audioMasterCallback audioMaster);
        virtual ~LaunchPlaySequencer();
        
        virtual VstPlugCategory getPlugCategory() { return kPlugCategSynth; }
        virtual bool getEffectName(char *name);
        virtual VstInt32 canDo(char *text);
	    
        virtual void open();
        virtual VstInt32 processEvents(VstEvents *events);
        virtual void processReplacing (float **inputs, float **outputs, VstInt32 sampleFrames);
    };
    
} // } namespace LaunchPlayVST

#endif