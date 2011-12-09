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
#include "MIDIHelper.h"

#include <memory>
#include <functional>
#include <vector>
#include <algorithm>

#define kStrideQuarter      1
#define kStrideEight        .5
#define kStrideSixteenth    .25
#define kLaunchPadWidth     8
#define kLaunchPadHeight    8

namespace LaunchPlayVST {

    class SequencerBase {
    public:
        virtual ~SequencerBase() = 0;
        virtual void init() = 0;
        virtual void processForward(double ppq, VstInt32 sampleOffset) = 0;
        virtual void processUserEvents(VstEvents *events) = 0;
        virtual VstEventsArray* flushMidiEvents() = 0;
        virtual VstEventsArray* flushFeedbackEvents() = 0;
    };
    
    enum GridDirection { up, down, left, right };
    
    struct Worker { 
        size_t x;
        size_t y;
        GridDirection direction;
    };
    
    typedef std::vector<Worker> WorkerList;
    typedef WorkerList::iterator WorkerListIter;
    
    class GridSequencer : public SequencerBase, boost::noncopyable {
    public:
        GridSequencer(size_t width, size_t height);
        virtual ~GridSequencer();
        
        virtual void processForward(double ppq, VstInt32 sampleOffset);
        virtual VstEventsArray* flushMidiEvents();
    protected:
        bool addWorker(size_t posX, size_t posY, GridDirection direction);
        WorkerListIter beginIterator();
        WorkerListIter endIterator();
        bool removeWorker(size_t posX, size_t posY);
        bool removeAllWorkers();
    private:
        struct EqualLocations : std::binary_function<Worker, Worker, bool> {
            bool operator()(Worker const& a, Worker const& b) const;
        };
        
        struct MoveForward : std::binary_function<Worker, Worker, void> {
            void operator()(Worker & worker, Worker const& boundaries) const;
        };
        
        struct HandleColision : std::binary_function<Worker, WorkerList, void> {
            void operator()(Worker & worker, WorkerList const& workerList) const;
        };

        WorkerList workers_;
        Worker boundaries_;
    };
    
    class LaunchPadSequencer : public GridSequencer {
        std::vector<VstEvent*> internalEvents_;
        GridDirection currentDirection_;
        bool primaryBufferEnabled_;
    protected:
        void resetLaunchPad(VstInt32 deltaFrames = 0);
        void setXYLayout(VstInt32 deltaFrames = 0);
        void setDirection(GridDirection direction, VstInt32 deltaFrames = 0);
        void swapBuffers(VstInt32 deltaFrames = 0);
    public:
        LaunchPadSequencer();
        virtual ~LaunchPadSequencer();
    
        virtual void init();
        virtual void processUserEvents(VstEvents *eventPtr);
        virtual void processForward(double ppq, VstInt32 sampleOffset);
        virtual VstEventsArray* flushFeedbackEvents();
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
        virtual VstInt32 processEvents(VstEvents *eventPtr);
        virtual void processReplacing (float **inputs, float **outputs, VstInt32 sampleFrames);
    };
    
} // } namespace LaunchPlayVST

#endif