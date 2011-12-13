//
//  LaunchPlay.cpp
//  LaunchPlayVST
//
//  Created by Fred G on 05/12/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include "LaunchPlay.h"

#include <math.h>

using namespace LaunchPlayVST;

#pragma mark LaunchPlayBase
LaunchPlayBase::LaunchPlayBase(audioMasterCallback audioMaster, 
                               VstInt32 numPrograms, 
                               VstInt32 numParams) 
    : AudioEffectX(audioMaster, numPrograms, numParams) 
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

VstInt32 LaunchPlayBase::getVendorVersion()
{ 
	return kVendorVersion; 
}

VstInt32 LaunchPlayBase::denormalizeValue(float value, VstInt32 max) {
	VstInt32 denormalizedValue(floor(value * max));
	return denormalizedValue;
}

float LaunchPlayBase::normalizeValue(VstInt32 value, VstInt32 max) {
	float normalizedValue(float(value) / float(max));
	return normalizedValue;
}