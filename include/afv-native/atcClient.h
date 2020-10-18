//
//  atcClient.hpp
//  afv_native
//
//  Created by Mike Evans on 10/17/20.
//

#ifndef atcClient_h
#define atcClient_h

#include <stdio.h>
#include <event2/event.h>
#include <string>

#include <map>

#include "afv-native/Client.h"








std::map<int,afv_native::audio::AudioDevice::DeviceInfo> outputDevices();
std::map<int,afv_native::audio::AudioDevice::DeviceInfo> inputDevices();
std::map<afv_native::audio::AudioDevice::Api, std::string> audioProviders();


std::string nameForAudioApi(afv_native::audio::AudioDevice::Api apiNum);


#ifdef __cplusplus
extern "C" {
#endif

void setAudioApi(afv_native::audio::AudioDevice::Api apiNum) ;
void atcClientCreate();



void voiceDump();
void eventLoop();
void setAFVUsername(std::string username);
void setAFVPassword(std::string password);
void setAFVCallsign(std::string callsign);
void connectVoice();
void disconnectVoice();


#ifdef __cplusplus
}
#endif


#endif /* atcClient_h */
