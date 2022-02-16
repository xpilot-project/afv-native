#include "MiniAudioDevice.h"

using namespace afv_native::audio;
using namespace std;

ma_result result;
ma_device_config config;
ma_device device;

MiniAudioDevice::MiniAudioDevice(const string& outputDeviceName, const string& inputDeviceName) :
    mOutputDeviceName(outputDeviceName),
    mInputDeviceName(inputDeviceName),
    mInitialized(false)
{

}

MiniAudioDevice::~MiniAudioDevice()
{
    ma_device_uninit(&device);
    mInitialized = false;
}

bool MiniAudioDevice::open()
{
    ma_device_id inputDeviceId;
    getDeviceForName(mInputDeviceName, true, inputDeviceId);

    ma_device_id outputDeviceId;
    getDeviceForName(mOutputDeviceName, false, outputDeviceId);

    config = ma_device_config_init(ma_device_type_duplex);
    config.capture.pDeviceID = &inputDeviceId;
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.playback.pDeviceID = &outputDeviceId;
    config.playback.channels = 1;
    config.playback.format = ma_format_f32;
    config.noClip = true;
    config.periodSizeInMilliseconds = frameLengthMs;
    config.sampleRate = sampleRateHz;
    config.dataCallback = maAudioCallback;
    config.pUserData = this;

    ma_result result = ma_device_init(NULL, &config, &device);
    if (result != MA_SUCCESS) {

    }

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {

    }

    return true;
}

std::list<ma_device_info*> MiniAudioDevice::getCompatibleInputDevices()
{
    std::list<ma_device_info*> deviceList;

    ma_result result;
    ma_context context;
    ma_device_info* pCaptureDeviceInfos;
    ma_uint32 captureDeviceCount;
    ma_uint32 iDevice;

    if (ma_context_init(NULL, 0, NULL, &context) == MA_SUCCESS) {
        result = ma_context_get_devices(&context, NULL, NULL, &pCaptureDeviceInfos, &captureDeviceCount);
        if (result == MA_SUCCESS) {
            for (iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
                deviceList.push_back(&pCaptureDeviceInfos[iDevice]);
            }
        }
    }

    ma_context_uninit(&context);

    return std::move(deviceList);
}

std::list<ma_device_info*> MiniAudioDevice::getCompatibleOutputDevices()
{
    std::list<ma_device_info*> deviceList;

    ma_result result;
    ma_context context;
    ma_device_info* pPlaybackDeviceInfos;
    ma_uint32 playbackDeviceCount;
    ma_uint32 iDevice;

    if (ma_context_init(NULL, 0, NULL, &context) == MA_SUCCESS) {
        result = ma_context_get_devices(&context, &pPlaybackDeviceInfos, &playbackDeviceCount, NULL, NULL);
        if (result == MA_SUCCESS) {
            for (iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
                deviceList.push_back(&pPlaybackDeviceInfos[iDevice]);
            }
        }
    }

    ma_context_uninit(&context);

    return std::move(deviceList);
}

bool MiniAudioDevice::getDeviceForName(const string& deviceName, bool forInput, ma_device_id& deviceId)
{
    auto allDevices = forInput ? getCompatibleInputDevices() : getCompatibleOutputDevices();

    if (!allDevices.empty()) {
        for (const auto& devicePair : allDevices) {
            if (devicePair->name == deviceName) {
                deviceId = devicePair->id;
                return true;
            }
        }
    }

    return false;
}

void MiniAudioDevice::maAudioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    auto* thisDev = reinterpret_cast<MiniAudioDevice*>(pDevice->pUserData);
    thisDev->audioCallback(pInput, pOutput, frameCount);
}

int MiniAudioDevice::audioCallback(const void* inputBuffer, void* outputBuffer, unsigned int nFrames)
{
    {
        std::lock_guard<std::mutex> sinkGuard(mSinkPtrLock);
        if (mSink && inputBuffer) {
            for (size_t i = 0; i < nFrames; i += frameSizeSamples) {
                mSink->putAudioFrame(reinterpret_cast<const float*>(inputBuffer) + i);
            }
        }
    }
    if (outputBuffer) {
        std::lock_guard<std::mutex> sourceGuard(mSourcePtrLock);
        for (size_t i = 0; i < nFrames; i += frameSizeSamples) {
            if (mSource) {
                SourceStatus rv;
                rv = mSource->getAudioFrame(reinterpret_cast<float*>(outputBuffer) + i);
                if (rv != SourceStatus::OK) {
                    ::memset(reinterpret_cast<float*>(outputBuffer) + i, 0, frameSizeBytes);
                    mSource.reset();
                }
            }
            else {
                // if there's no source, but there is an output buffer, zero it to avoid making horrible buzzing sounds.
                ::memset(reinterpret_cast<float*>(outputBuffer) + i, 0, frameSizeBytes);
            }
        }
    }
    return 0;
}

/* ========== Factory hooks ============= */

bool MiniAudioDevice::getDeviceForName(const string& deviceName, bool forInput, ma_device_id& deviceId)
{
    auto allDevices = forInput ? getCompatibleInputDevices() : getCompatibleOutputDevices();

    if (!allDevices.empty()) {
        for (const auto& devicePair : allDevices) {
            if (devicePair->name == deviceName) {
                deviceId = devicePair->id;
                return true;
            }
        }
    }

    return false;
}

list<ma_device_info*> AudioDevice::getCompatibleInputDevices() {
    auto allDevices = MiniAudioDevice::getCompatibleInputDevices();
    list<ma_device_info*> returnDevices;
    for (const auto& p : allDevices) {
        returnDevices.push_back(p);
    }
    return returnDevices;
}

list<ma_device_info*> AudioDevice::getCompatibleOutputDevices() {
    auto allDevices = MiniAudioDevice::getCompatibleOutputDevices();
    list<ma_device_info*> returnDevices;
    for (const auto& p : allDevices) {
        returnDevices.push_back(p);
    }
    return returnDevices;
}

std::shared_ptr<AudioDevice>
AudioDevice::makeDevice(
    const std::string& outputDevice,
    const std::string& inputDevice) {
    auto devsp = std::make_shared<MiniAudioDevice>(outputDevice, inputDevice);
    return devsp;
}