#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>

#include "afv-native/Client.h"
#include <event2/event.h>

namespace clientapi
{
    #ifdef WIN32
    #define AFV_API __declspec(dllexport)
    #else
    #define AFV_API
    #endif

    struct event_base* ev_base;

    std::mutex afvMutex;
    std::unique_ptr<afv_native::Client> client;
    std::unique_ptr<std::thread> eventThread;
    std::atomic<bool>keepAlive{ false };
    std::atomic<bool> isInitialized{ false };
    afv_native::audio::AudioDevice::Api mAudioApi;
    int mInputDevice;
    int mOutputDevice;

    typedef std::map<afv_native::audio::AudioDevice::Api, std::string> ApiProviderMap;
    typedef std::map<int, afv_native::audio::AudioDevice::DeviceInfo> AudioDeviceMap;

    std::map<afv_native::audio::AudioDevice::Api, std::string> mAudioProviders;
    std::map<int, afv_native::audio::AudioDevice::DeviceInfo> mInputDevices;
    std::map<int, afv_native::audio::AudioDevice::DeviceInfo> mOutputDevices;
}

using namespace clientapi;

#ifdef __cplusplus
extern "C" {

    AFV_API void Client_Init(char* resourcePath, int numRadios, char* clientName)
    {
        #ifdef WIN32
        WORD wVersionRequested;
        WSADATA wsaData;
        wVersionRequested = MAKEWORD(2, 2);
        WSAStartup(wVersionRequested, &wsaData);
        #endif

        ev_base = event_base_new();
        client = std::make_unique<afv_native::Client>(ev_base, resourcePath, numRadios, clientName);
        mAudioProviders = afv_native::audio::AudioDevice::getAPIs();

        keepAlive = true;
        eventThread = std::make_unique<std::thread>([]
        {
            while (keepAlive)
            {
                event_base_loop(ev_base, EVLOOP_NONBLOCK);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        isInitialized = true;
    }

    AFV_API void Client_Destroy()
    {
        keepAlive = false;
        if (eventThread->joinable())
        {
            eventThread->join();
        }
        client.reset();
        isInitialized = false;
        #ifdef WIN32
        WSACleanup();
        #endif
    }

    AFV_API bool Client_IsInitialized()
    {
        return isInitialized;
    }

    AFV_API void Client_SetBaseUrl(const char* newUrl)
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        client->setBaseUrl(std::string(newUrl));
    }

    AFV_API void Client_SetClientPosition(double lat, double lon, double amslm, double aglm)
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        client->setClientPosition(lat, lon, amslm, aglm);
    }

    AFV_API void Client_SetRadioState(unsigned int radioNum, int freq)
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        client->setRadioState(radioNum, freq);
    }

    AFV_API void Client_SetTxRadio(unsigned int radioNum)
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        client->setTxRadio(radioNum);
    }

    AFV_API void Client_SetRadioGain(unsigned int radioNum, float gain)
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        client->setRadioGain(radioNum, gain);
    }

    AFV_API void Client_SetPtt(bool pttState)
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        client->setPtt(pttState);
    }

    AFV_API void Client_SetCredentials(const char* username, const char* password)
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        client->setCredentials(std::string(username), std::string(password));
    }

    AFV_API void Client_SetCallsign(const char* callsign)
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        client->setCallsign(std::string(callsign));
    }

    AFV_API bool Client_IsVoiceConnected()
    {
        return client->isVoiceConnected();
    }

    AFV_API bool Client_IsApiConnected()
    {
        return client->isAPIConnected();
    }

    AFV_API bool Client_Connect()
    {
        return client->connect();
    }

    AFV_API void Client_Disconnect()
    {
        client->disconnect();
    }

    AFV_API double Client_GetInputPeak()
    {
        return client->getInputPeak();
    }

    AFV_API double Client_GetInputVu()
    {
        return client->getInputVu();
    }

    AFV_API bool Client_GetInputFiltersEnabled()
    {
        return client->getEnableInputFilters();
    }

    AFV_API void Client_SetEnableInputFilters(bool enableInputFilters)
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        client->setEnableInputFilters(enableInputFilters);
    }

    AFV_API void Client_SetEnableOutputEffects(bool enableEffects)
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        client->setEnableOutputEffects(enableEffects);
    }

    AFV_API void Client_EnableHfSquelch(bool enableSquelch)
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        client->setEnableHfSquelch(enableSquelch);
    }

    AFV_API void Client_StartAudio()
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        client->startAudio();
    }

    AFV_API void Client_StopAudio()
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        client->stopAudio();
    }

    AFV_API void Client_GetAudioApis(void(*callback)(unsigned int api, const char* name))
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        for (const auto& item : mAudioProviders)
        {
            callback(item.first, item.second.c_str());
        }
    }

    AFV_API void Client_SetAudioApi(const char* api)
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        for (ApiProviderMap::const_iterator it = mAudioProviders.begin(); it != mAudioProviders.end(); ++it)
        {
            if (it->second == api)
            {
                mAudioApi = it->first;
                mInputDevices = afv_native::audio::AudioDevice::getCompatibleInputDevicesForApi(mAudioApi);
                mOutputDevices = afv_native::audio::AudioDevice::getCompatibleOutputDevicesForApi(mAudioApi);
            }
        }
    }

    AFV_API void Client_SetAudioDevice()
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        {
            client->setAudioApi(mAudioApi);
            try
            {
                client->setAudioInputDevice(mInputDevices.at(mInputDevice).id);
            }
            catch (std::out_of_range&)
            {
            }
            try
            {
                client->setAudioOutputDevice(mOutputDevices.at(mOutputDevice).id);
            }
            catch (std::out_of_range&)
            {
            }
        }
    }

    AFV_API void Client_GetInputDevice(void(*callback)(unsigned int id, const char* name))
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        for (const auto& item : mInputDevices)
        {
            callback(item.first, item.second.name.c_str());
        }
    }

    AFV_API void Client_GetOutputDevice(void(*callback)(unsigned int id, const char* name))
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        for (const auto& item : mOutputDevices)
        {
            callback(item.first, item.second.name.c_str());
        }
    }

    AFV_API void Client_SetAudioInputDevice(const char* inputDevice)
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        for (AudioDeviceMap::const_iterator it = mInputDevices.begin(); it != mInputDevices.end(); ++it)
        {
            if (strcmp(it->second.name.c_str(), inputDevice) == 0)
            {
                mInputDevice = it->first;
                client->setAudioInputDevice(it->second.name.c_str());
            }
        }
    }

    AFV_API void Client_SetAudioOutputDevice(const char* outputDevice)
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        for (AudioDeviceMap::const_iterator it = mOutputDevices.begin(); it != mOutputDevices.end(); ++it)
        {
            if (strcmp(it->second.name.c_str(), outputDevice) == 0)
            {
                mOutputDevice = it->first;
                client->setAudioOutputDevice(it->second.name.c_str());
            }
        }
    }

    AFV_API void Client_GetStationAliases(void(*callback)(const char* id, const char* callsign,
        unsigned int frequency, unsigned int frequencyAlias))
    {
        std::lock_guard<std::mutex> lock(afvMutex);
        for (const auto& station : client->getStationAliases())
        {
            callback(station.ID.c_str(), station.Name.c_str(), station.Frequency, station.FrequencyAlias);
        }
    }

    AFV_API void Client_RaiseClientEvent(void(*callback)(afv_native::ClientEventType evt, int data))
    {
        client->ClientEventCallback.addCallback(nullptr, [callback](afv_native::ClientEventType evt, void* data)
        {
            int v = -1;
            if (data != nullptr)
            {
                v = *reinterpret_cast<int*>(data);
            }
            callback(evt, v);
        });
    }

    AFV_API bool Client_IsCom1Rx()
    {
        return client->getRadioSimulation()->AudiableAudioStreams[0].load() > 0;
    }

    AFV_API bool Client_IsCom2Rx()
    {
        return client->getRadioSimulation()->AudiableAudioStreams[1].load() > 0;
    }
}
#endif