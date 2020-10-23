//
//  ATCRadioStack.h
//  afv_native
//
//  Created by Mike Evans on 10/21/20.
//

#ifndef ATCRadioStack_h
#define ATCRadioStack_h

#include <memory>
#include <unordered_map>

#include "afv-native/utility.h"
#include "afv-native/afv/EffectResources.h"
#include "afv-native/afv/RemoteVoiceSource.h"
#include "afv-native/afv/RollingAverage.h"
#include "afv-native/afv/VoiceCompressionSink.h"
#include "afv-native/afv/dto/voice_server/AudioRxOnTransceivers.h"
#include "afv-native/afv/dto/Transceiver.h"
//#include "afv-native/audio/ISampleSink.h"
//#include "afv-native/audio/ISampleSource.h"
//#include "afv-native/audio/OutputMixer.h"
#include "afv-native/audio/PinkNoiseGenerator.h"
#include "afv-native/audio/SineToneSource.h"
//#include "afv-native/audio/SpeexPreprocessor.h"
#include "afv-native/audio/VHFFilterSource.h"
#include "afv-native/cryptodto/UDPChannel.h"
#include "afv-native/event/EventCallbackTimer.h"


namespace afv_native {
    namespace afv {
    
        /** RadioState is the internal state object for each radio within a RadioSimulation.
         *
         * It tracks the current playback position of the mixing effects, the channel frequency and gain, and the transceivers.
         */
        class ATCRadioState {
        public:
            unsigned int Frequency;
            float Gain;
            std::shared_ptr<audio::RecordedSampleSource> Click;
            std::shared_ptr<audio::PinkNoiseGenerator> WhiteNoise;
            std::shared_ptr<audio::RecordedSampleSource> Crackle;
            std::shared_ptr<audio::SineToneSource> BlockTone;
            audio::VHFFilterSource vhfFilter;
            int mLastRxCount;
            bool mBypassEffects;
            bool onHeadset; // If we're not on the headset, we're on the Speaker
            bool tx;
            bool rx;
            std::vector<dto::Transceiver> transceivers;
        };

    
    
        class ATCRadioStack:
            public ICompressedFrameSink
        {
        public:
            ATCRadioStack(struct event_base * evBase,
                          cryptodto::UDPChannel *channel);
            virtual ~ATCRadioStack();
            void rxVoicePacket(const afv::dto::AudioRxOnTransceivers &pkt);
            void setPtt(bool pressed);
            void setUDPChannel(cryptodto::UDPChannel *newChannel);
            void setCallsign(const std::string &newCallsign);
            
            void addFrequency(unsigned int freq, std::vector<dto::Transceiver> transceivers, bool onHeadset);
            
            bool getTxActive(unsigned int freq);
            bool getRxActive(unsigned int freq);
            
            void setRx(unsigned int freq, bool rx);
            void setTx(unsigned int freq, bool tx);
            void setOnHeadset(unsigned int freq, bool onHeadset);
            void setGain(unsigned int freq, float gain);
            
            double getVu() const;
            double getPeak() const;
            void reset();
            
            std::atomic<uint32_t> IncomingAudioStreams;
            
        protected:
            
            static const int maintenanceTimerIntervalMs = 30 * 1000;
            struct event_base *mEvBase;
            cryptodto::UDPChannel *mChannel;
            std::string mCallsign;
            
            std::mutex mStreamMapLock;
            std::unordered_map<std::string, struct CallsignMeta> mIncomingStreams;
            
            std::mutex mRadioStateLock;
            std::map<unsigned int, ATCRadioState> mRadioState;
            
            std::atomic<bool> mPtt;
            bool mLastFramePtt;
            
            std::atomic<uint32_t> mTxSequence;
            std::shared_ptr<VoiceCompressionSink> mVoiceSink;
            
            event::EventCallbackTimer mMaintenanceTimer;
            RollingAverage<double> mVuMeter;
            void processCompressedFrame(std::vector<unsigned char> compressedData);
            void maintainIncomingStreams();
    
        };

    
    
    }
}



#endif /* ATCRadioStack_h */
