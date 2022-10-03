#include "MiniAudioDevice.h"

#include <QDebug>
#include <memory>
#include <cstring>

using namespace afv_native::audio;
using namespace std;

MiniAudioDevice::MiniAudioDevice(
        const std::string& userStreamName,
        const std::string& outputDeviceName,
        const std::string& inputDeviceName,
        AudioDevice::Api audioApi) :
    AudioDevice(),
    mUserStreamName(userStreamName),
    mOutputDeviceName(outputDeviceName),
    mInputDeviceName(inputDeviceName),
    mInitialized(false)
{
    ma_context_init(NULL, 0, NULL, &context);
}

MiniAudioDevice::~MiniAudioDevice()
{

}

bool MiniAudioDevice::open()
{
    return initializeAudio();
}

void MiniAudioDevice::close()
{
    if(mInitialized) {
        ma_device_uninit(&device);
    }
    ma_context_uninit(&context);
    mInitialized = false;
}

std::map<int, ma_device_info> MiniAudioDevice::getCompatibleInputDevicesForApi(Api api)
{
    std::map<int, ma_device_info> deviceList;

    ma_device_info* devices;
    ma_uint32 deviceCount;
    ma_context context;

    if (ma_context_init(NULL, 0, NULL, &context) == MA_SUCCESS) {
        auto result = ma_context_get_devices(&context, NULL, NULL, &devices, &deviceCount);
        if(result == MA_SUCCESS) {
            for(ma_uint32 i = 0; i < deviceCount; i++) {
                deviceList.emplace(i, devices[i]);
            }
        }
    }

    ma_context_uninit(&context);

    return deviceList;
}

std::map<int, ma_device_info> MiniAudioDevice::getCompatibleOutputDevicesForApi(Api api)
{
    std::map<int, ma_device_info> deviceList;

    ma_device_info* devices;
    ma_uint32 deviceCount;
    ma_context context;

    if (ma_context_init(NULL, 0, NULL, &context) == MA_SUCCESS) {
        auto result = ma_context_get_devices(&context, &devices, &deviceCount, NULL, NULL);
        if(result == MA_SUCCESS) {
            for(ma_uint32 i = 0; i < deviceCount; i++) {
                deviceList.emplace(i, devices[i]);
            }
        }
    }

    ma_context_uninit(&context);

    return deviceList;
}

bool MiniAudioDevice::initializeAudio()
{
    if(mInitialized) {
        ma_device_uninit(&device);
    }

    ma_device_id inputDeviceId;
    getDeviceForName(mInputDeviceName, true, inputDeviceId);

    ma_device_id outputDeviceId;
    getDeviceForName(mOutputDeviceName, false, outputDeviceId);

    ma_context_config contextConfig = ma_context_config_init();
    contextConfig.threadPriority = ma_thread_priority_realtime;
    contextConfig.jack.pClientName = "xPilot";
    contextConfig.pulse.pApplicationName = "xPilot";

    ma_device_config cfg = ma_device_config_init(ma_device_type_duplex);

    cfg.playback.pDeviceID = &outputDeviceId;
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 1;
    cfg.playback.shareMode = ma_share_mode_exclusive;

    cfg.capture.pDeviceID = &inputDeviceId;
    cfg.capture.channels = 1;
    cfg.capture.format = ma_format_f32;

    cfg.sampleRate = sampleRateHz;
    cfg.periodSizeInMilliseconds = frameLengthMs;
    cfg.pUserData = this;
    cfg.dataCallback = maAudioCallback;

    if(ma_device_init(&context, &cfg, &device) != MA_SUCCESS) {
        qDebug() << "Couldn't initialize audio device";
        return false;
    }

    if(ma_device_start(&device) != MA_SUCCESS) {
        qDebug() << "Could not start audio device";
        return false;
    }

    mInitialized = true;
    return true;
}

bool MiniAudioDevice::getDeviceForName(const std::string &deviceName, bool forInput, ma_device_id &deviceId)
{
    auto allDevices = forInput ? getCompatibleInputDevicesForApi(0) : getCompatibleOutputDevicesForApi(0);

    if(!allDevices.empty()) {
        for(const auto& devicePair : allDevices) {
            if(devicePair.second.name == deviceName) {
                deviceId = devicePair.second.id;
                return true;
            }
        }
    }

    return false;
}

void MiniAudioDevice::maAudioCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    auto device = reinterpret_cast<MiniAudioDevice*>(pDevice->pUserData);
    device->audioCallback(pInput, pOutput, frameCount);
}

int MiniAudioDevice::audioCallback(const void *inputBuffer, void *outputBuffer, unsigned int nFrames)
{
    if (outputBuffer) {
        std::lock_guard<std::mutex> sourceGuard(mSourcePtrLock);

        for (size_t i = 0; i < nFrames; i += frameSizeSamples) {
            if (mSource) {
                SourceStatus rv;
                rv = mSource->getAudioFrame(reinterpret_cast<float *>(outputBuffer) + i);
                if (rv != SourceStatus::OK) {
                    ::memset(reinterpret_cast<float *>(outputBuffer) + i, 0, frameSizeBytes);
                    mSource.reset();
                }
            } else {
                // if there's no source, but there is an output buffer, zero it to avoid making horrible buzzing sounds.
                ::memset(reinterpret_cast<float *>(outputBuffer) + i, 0, frameSizeBytes);
            }
        }
    }

    {
        std::lock_guard<std::mutex> sinkGuard(mSinkPtrLock);
        if (mSink && inputBuffer) {
            for (size_t i = 0; i < nFrames; i += frameSizeSamples) {
                mSink->putAudioFrame(reinterpret_cast<const float *>(inputBuffer) + i);
            }
        }
    }

    return 0;
}

/* ========== Factory hooks ============= */

map<AudioDevice::Api, std::string> AudioDevice::getAPIs() {
    return MiniAudioDevice::getAPIs();
}

map<int, AudioDevice::DeviceInfo> AudioDevice::getCompatibleInputDevicesForApi(AudioDevice::Api api) {
    auto allDevices = MiniAudioDevice::getCompatibleInputDevicesForApi(api);
    map<int, AudioDevice::DeviceInfo> returnDevices;
    for (const auto &p: allDevices) {
        returnDevices.emplace(p.first, AudioDevice::DeviceInfo(p.second.name));
    }
    return returnDevices;
}

map<int, AudioDevice::DeviceInfo> AudioDevice::getCompatibleOutputDevicesForApi(AudioDevice::Api api) {
    auto allDevices = MiniAudioDevice::getCompatibleOutputDevicesForApi(api);
    map<int, AudioDevice::DeviceInfo> returnDevices;
    for (const auto &p: allDevices) {
        returnDevices.emplace(p.first, AudioDevice::DeviceInfo(p.second.name));
    }
    return returnDevices;
}

std::shared_ptr<AudioDevice>
AudioDevice::makeDevice(
        const std::string &userStreamName,
        const std::string &outputDeviceId,
        const std::string &inputDeviceId,
        AudioDevice::Api audioApi) {
    auto devsp = std::make_shared<MiniAudioDevice>(userStreamName, outputDeviceId, inputDeviceId, audioApi);
    return devsp;
}
