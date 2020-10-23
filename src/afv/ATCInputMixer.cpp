//
//  ATCInputMixer.cpp
//  afv_native
//
//  Created by Mike Evans on 10/22/20.
//



#include "afv-native/afv/ATCInputMixer.h"
#include "afv-native/afv/ATCRadioStack.h"
#include "afv-native/afv/RadioSimulation.h"

using namespace afv_native::afv;
using namespace afv_native;
using namespace afv_native::audio;


InputInPort::InputInPort()
{
    
}

void InputInPort::putAudioFrame(const SampleType *bufferIn)
{
    
    
}

InputOutPort::InputOutPort()
{
    
}




ATCInputMixer::ATCInputMixer() :
    mInputs()

{
    
}

ATCInputMixer::~ATCInputMixer()
{
    
    
}

audio::ISampleSink ATCInputMixer::attachInputDevice(unsigned int port)
{
    
}

void ATCInputMixer::attachOutput(unsigned int port, std::shared_ptr<audio::ISampleSink> sink)
{
    
}

bool ATCInputMixer::hasMixerConnection(unsigned int srcport, unsigned int dstport)
{
    
}







