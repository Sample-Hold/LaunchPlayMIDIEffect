//
//  Mindstorm.h
//  LaunchPlayVST
//
//  Created by Fred G on 06/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef LaunchPlayVST_Mindstorm_h
#define LaunchPlayVST_Mindstorm_h

#include <audioeffectx.h>

namespace LaunchPlayVST {
    
    enum WorkerDirection { left, right, up, down };
    enum Scale { major, minor };
    enum Interval { thirdup, thirddown, fifthup, fifthdown};

    class GridSequencer
    {
    public:
        GridSequencer(VstInt32 gridSize, VstInt32 noteNumber, Scale scale, Interval interval);
        virtual ~GridSequencer();
        
        void addWorker(VstInt32 posX, VstInt32 posY, WorkerDirection direction);
        void removeWorker(VstInt32 posX, VstInt32 posY);
        void removeAllWorkers();

        void processForward(double ppq, double deltaFrames);
    };
}


#endif
