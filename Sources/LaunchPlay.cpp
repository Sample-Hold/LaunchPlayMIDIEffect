//
//  LaunchPlay.cpp
//  LaunchPlayVST
//
//  Created by Fred G on 05/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include "LaunchPlay.h"

#include <math.h>

#define BEATSPERSAMPLE(tempo, sampleRate) (tempo) / (sampleRate) / 60.0

using namespace LaunchPlayVST;

#pragma mark LaunchPlayBase
LaunchPlayBase::LaunchPlayBase(audioMasterCallback audioMaster, 
                               VstInt32 numPrograms, 
                               VstInt32 numParams) 
    : AudioEffectX(audioMaster, numPrograms, numParams),
	currentTempo_(kDefaultTempo), 
	currentBeatsPerSample_(BEATSPERSAMPLE(currentTempo_, sampleRate))
{

}

LaunchPlayBase::~LaunchPlayBase() 
{

}

bool LaunchPlayBase::getProductString(char* text)
{
	vst_strncpy (text, kProductString, kVstMaxProductStrLen);
	return true;
}

bool LaunchPlayBase::getVendorString(char* text)
{
	vst_strncpy (text, kVendorString, kVstMaxVendorStrLen);
	return true;
}

void LaunchPlayBase::detectTicks(VstTimeInfo *timeInfo, 
                                      VstInt32 sampleFrames, 
                                      double stride)
{
    double startPpqPos = timeInfo->ppqPos;

	if(timeInfo->tempo != currentTempo_ || timeInfo->sampleRate != sampleRate) {
		currentTempo_ = timeInfo->tempo;
		currentBeatsPerSample_ = BEATSPERSAMPLE(currentTempo_, timeInfo->sampleRate);
	}

    double endPpqPos = startPpqPos + sampleFrames * currentBeatsPerSample_;
    
    double ppqPos = ceil(startPpqPos);
    while (ppqPos - stride > startPpqPos)
        ppqPos -= stride;
    
    for(;ppqPos < endPpqPos; ppqPos += stride)
        onTick(currentTempo_, ppqPos, timeInfo->sampleRate, VstInt32((ppqPos - startPpqPos) / currentBeatsPerSample_));
}

VstInt32 LaunchPlayBase::getVendorVersion()
{ 
	return kVendorVersion; 
}

VstInt32 LaunchPlayBase::denormalizeValue(float const value, VstInt32 const max) {
	VstInt32 denormalizedValue = (VstInt32) floor((value * max) + .5);
	return denormalizedValue;
}

float LaunchPlayBase::normalizeValue(VstInt32 const value, const VstInt32 max) {
	float normalizedValue(float(value) / float(max));
	return normalizedValue;
}