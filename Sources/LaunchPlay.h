//
//  LaunchPlay.h
//  LaunchPlayVST
//
//  Created by Fred G on 05/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef LaunchPlayVST_LaunchPlay_h
#define LaunchPlayVST_LaunchPlay_h

#include <assert.h>
#include <memory>
#include <audioeffectx.h>
#include <boost/utility.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

#define kProductString      "LaunchPlay sequencer for LaunchPad"
#define kVendorString       "Fred G for sample-hold.com"
#define kSequencerName      "LaunchPlaySequencer"
#define kEmitterName        "LaunchPlayEmitter"
#define kMessageQueueName   "LaunchPlayMessageQueue"
#define kMessageQueueSize   32
#define kVendorVersion      1000
#define kSeqUniqueID        CCONST('f','9', 's', 'q')
#define kEmUniqueID         CCONST('f','9', 'e', 'm')

namespace LaunchPlayVST {
    
    class LaunchPlayBase : public AudioEffectX, boost::noncopyable {

    public:
        LaunchPlayBase(audioMasterCallback audioMaster, 
                       VstInt32 numPrograms, 
                       VstInt32 numParams);
        
        virtual bool getVendorString(char *text);
        virtual bool getProductString(char *text);
        virtual VstInt32 getVendorVersion();
    };
    
} // } namespace LaunchPlayVST

#endif