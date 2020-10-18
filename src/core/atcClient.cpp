//
//  atcClient.cpp
//  afv_native
//
//  Created by Mike Evans on 10/17/20.
//

#include "afv-native/atcClient.h"
#include "afv-native/Client.h"
#include <memory>


int mInputDevice;
int mOutputDevice;
struct event_base *ev_base;
std::shared_ptr<afv_native::Client> mClient;
afv_native::audio::AudioDevice::Api mAudioApi;
std::map<int,afv_native::audio::AudioDevice::DeviceInfo> mInputDevices;
std::map<int,afv_native::audio::AudioDevice::DeviceInfo> mOutputDevices;

double mClientLatitude = 33.942791;
double mClientLongitude = -118.410042;
double mClientAltitudeMSLM = 100.0;
double mClientAltitudeAGLM = 100.0;

int mTxRadio = 0;
int mCom1Freq = 124500000;
float mCom1Gain = 1.0f;
int mCom2Freq = 125800000;
float mCom2Gain = 1.0f;
bool mPTT = false;

bool mInputFilter = false;
bool mOutputEffects = false;
float mPeak = 0.0f;
float mVu = 0.0f;

std::string mAFVUsername = "";
std::string mAFVPassword = "";
std::string mAFVCallsign = "";


std::map<int,afv_native::audio::AudioDevice::DeviceInfo> inputDevices()
{
    return mInputDevices;
    
}

std::map<int,afv_native::audio::AudioDevice::DeviceInfo> outputDevices()
{
    return mOutputDevices;
}

void setAudioApi(afv_native::audio::AudioDevice::Api apiNum) {
    mAudioApi = apiNum;
    mInputDevices = afv_native::audio::AudioDevice::getCompatibleInputDevicesForApi(mAudioApi);
    mOutputDevices = afv_native::audio::AudioDevice::getCompatibleOutputDevicesForApi(mAudioApi);
}

void setAudioDevice() {
    mClient->setAudioApi(mAudioApi);
    try {
        mClient->setAudioInputDevice(mInputDevices.at(mInputDevice).id);
    } catch (std::out_of_range &) {}
    try {
        mClient->setAudioOutputDevice(mOutputDevices.at(mOutputDevice).id);
    } catch (std::out_of_range &) {}
}

void atcClientCreate()
{
    ev_base = event_base_new();
    mClient = std::make_shared<afv_native::Client>(
            ev_base,
            ".",
            2,
            "MSRC 0.1");
    
    setAudioApi(0);
    
}


std::map<afv_native::audio::AudioDevice::Api, std::string> audioProviders()
{
    return afv_native::audio::AudioDevice::getAPIs();
}


std::string nameForAudioApi(afv_native::audio::AudioDevice::Api apiNum) {
    auto mAudioProviders = afv_native::audio::AudioDevice::getAPIs();
    auto mIter = mAudioProviders.find(apiNum);
    if (mIter == mAudioProviders.end()) {
        if (apiNum == 0) return "UNSPECIFIED (default)";
        return "INVALID";
    }
    return mIter->second;
}

void voiceDump()
{
    
    const char *currentInputDevice = "None Selected";
    if (mInputDevices.find(mInputDevice) != mInputDevices.end()) {
        currentInputDevice = mInputDevices.at(mInputDevice).name.c_str();
    }
    const char *currentOutputDevice = "None Selected";
    if (mOutputDevices.find(mOutputDevice) != mOutputDevices.end()) {
        currentOutputDevice = mOutputDevices.at(mOutputDevice).name.c_str();
    }
    
    printf("Input: %s",currentInputDevice);
    printf("Output: %s",currentOutputDevice);
}


void eventLoop()
{
    if(ev_base)
    {
        event_base_loop(ev_base, EVLOOP_NONBLOCK);
    }
}

void setAFVUsername(std::string username)
{
    mAFVUsername=std::move(username);
}

void setAFVPassword(std::string password)
{
    mAFVPassword=std::move(password);
}

void setAFVCallsign(std::string callsign)
{
    mAFVCallsign=std::move(callsign);
}



void connectVoice()
{
    setAudioDevice();
    //mClient->setBaseUrl(mAFVAPIServer);
    mClient->setClientPosition(mClientLatitude, mClientLongitude, mClientAltitudeMSLM, mClientAltitudeAGLM);
    mClient->setRadioState(0, mCom1Freq);
    mClient->setRadioState(1, mCom2Freq);
    mClient->setTxRadio(mTxRadio);
    mClient->setRadioGain(0, mCom1Gain);
    mClient->setRadioGain(1, mCom2Gain);
    mClient->setCredentials(mAFVUsername, mAFVPassword);
    mClient->setCallsign(mAFVCallsign);
    mClient->setEnableInputFilters(mInputFilter);
    mClient->setEnableOutputEffects(mOutputEffects);
    mClient->connect();
    
    
}


void disconnectVoice()
{
    mClient->disconnect();
    
}


