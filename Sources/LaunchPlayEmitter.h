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

namespace LaunchPlayVST {
    
    class LaunchPlayEmitter : public LaunchPlayBase {
    public:
        LaunchPlayEmitter(audioMasterCallback audioMaster);
        virtual ~LaunchPlayEmitter();
        
        virtual VstPlugCategory getPlugCategory() { return kPlugCategEffect; }
        virtual bool getEffectName(char *name);
        virtual VstInt32 canDo(char *text);
        
        virtual void processReplacing (float **inputs, float **outputs, VstInt32 sampleFrames);
    };
    
} // } namespace LaunchPlayVST

#endif
