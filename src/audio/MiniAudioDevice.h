#ifndef MINIAUDIO_DEVICE_H
#define MINIAUDIO_DEVICE_H

#define MINIAUDIO_IMPLEMENTATION
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI
#define MA_ENABLE_ALSA
#define MA_ENABLE_COREAUDIO
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
			explicit MiniAudioDevice(const std::string& outputDeviceName, const std::string& inputDeviceName);
			virtual ~MiniAudioDevice();

			bool open() override;
			void close() override;

			static std::list<ma_device_info*> getCompatibleInputDevices();
			static std::list<ma_device_info*> getCompatibleOutputDevices();

		protected:
			std::string mOutputDeviceName;
			std::string mInputDeviceName;
			bool mInitialized;

			bool getDeviceForName(const std::string& deviceName, bool forInput, ma_device_id& deviceId);

		private:
			static void maAudioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
			int audioCallback(const void* inputBuffer, void* outputBuffer, unsigned int nFrames);
		};
	}
}

#endif // !MINIAUDIO_DEVICE_H