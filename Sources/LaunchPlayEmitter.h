//
//  LaunchPlayEmitter.h
//  LaunchPlayVST
//
//  Created by Fred G on 07/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef LaunchPlayVST_LaunchPlayEmitter_h
#define LaunchPlayVST_LaunchPlayEmitter_h

#include "LaunchPlay.h"
#include "MIDIHelpers.h"

#include <boost/interprocess/ipc/message_queue.hpp>
#include <memory>

namespace LaunchPlayVST {
    
    class LaunchPlayEmitter : public LaunchPlayBase {
        std::auto_ptr<boost::interprocess::message_queue> mq_;
    public:
        LaunchPlayEmitter(audioMasterCallback audioMaster);
        ~LaunchPlayEmitter();
        
        VstPlugCategory getPlugCategory() { return kPlugCategEffect; }
        bool getEffectName(char *name);
        VstInt32 canDo(char *text);
        
        void open();
        void close();
        void processReplacing (float **inputs, float **outputs, VstInt32 sampleFrames);
    };
    
} // } namespace LaunchPlayVST

#endif
