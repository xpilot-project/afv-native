#ifndef MINIAUDIO_DEVICE_H
#define MINIAUDIO_DEVICE_H

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "afv-native/audio/AudioDevice.h"

#include <map>

namespace afv_native
{
    namespace audio
    {
        class MiniAudioDevice : public AudioDevice
        {
        public:
            explicit MiniAudioDevice(
                    const std::string& userStreamName,
                    const std::string& outputDeviceName,
                    const std::string& inputDeviceName,
                    Api audioApi);
            virtual ~MiniAudioDevice();

            bool open() override;
            void close() override;

            static std::map<int, ma_device_info> getCompatibleInputDevicesForApi(AudioDevice::Api api);
            static std::map<int, ma_device_info> getCompatibleOutputDevicesForApi(AudioDevice::Api api);

        private:
            bool initializeAudio();
            bool getDeviceForName(const std::string& deviceName, bool forInput, ma_device_id& deviceId);
            static void maAudioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
            int audioCallback(const void* inputBuffer, void* outputBuffer, unsigned int nFrames);

        private:
            std::string mUserStreamName;
            std::string mOutputDeviceName;
            std::string mInputDeviceName;
            bool mInitialized;
            ma_context context;
            ma_device device;
        };
    }
}

#endif // !MINIAUDIO_DEVICE_H
