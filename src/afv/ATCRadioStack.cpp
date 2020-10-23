//
//  ATCRadioStack.cpp
//  afv_native
//
//  Created by Mike Evans on 10/21/20.
//

#include <cmath>
#include <atomic>

#include "afv-native/Log.h"
#include "afv-native/afv/ATCRadioStack.h"
#include "afv-native/afv/RadioSimulation.h"
#include "afv-native/afv/dto/voice_server/AudioTxOnTransceivers.h"
#include "afv-native/audio/VHFFilterSource.h"
#include "afv-native/audio/PinkNoiseGenerator.h"


using namespace afv_native;
using namespace afv_native::afv;

extern const float fxClickGain;
extern const float fxBlockToneGain;
extern const float fxBlockToneFreq;
extern const float fxWhiteNoiseGain;


ATCRadioStack::ATCRadioStack(struct event_base * evBase,
                             cryptodto::UDPChannel *channel):
    mEvBase(evBase),
    mChannel(),
    mPtt(false),
    IncomingAudioStreams(0),
    mStreamMapLock(),
    mIncomingStreams(),
    mLastFramePtt(false),
    mRadioStateLock(),
    mTxSequence(0),
    mRadioState(),
    mVoiceSink(std::make_shared<VoiceCompressionSink>(*this)),
    mMaintenanceTimer(mEvBase, std::bind(&ATCRadioStack::maintainIncomingStreams, this)),
    mVuMeter(300 / audio::frameLengthMs)

{
    setUDPChannel(channel);
    mMaintenanceTimer.enable(maintenanceTimerIntervalMs);
    
}

ATCRadioStack::~ATCRadioStack()
{
//    delete[] mFetchBuffer;
//    delete[] mMixingBuffer;
//    delete[] mChannelBuffer;
//    delete[] AudiableAudioStreams;
}


void ATCRadioStack::rxVoicePacket(const afv::dto::AudioRxOnTransceivers &pkt)
{
    std::lock_guard<std::mutex> streamMapLock(mStreamMapLock);
    //FIXME:  Deal with the case of a single-callsign transmitting multiple different voicestreams simultaneously.
    mIncomingStreams[pkt.Callsign].source->appendAudioDTO(pkt);
    mIncomingStreams[pkt.Callsign].transceivers = pkt.Transceivers;
}

void ATCRadioStack::setUDPChannel(cryptodto::UDPChannel *newChannel)
{
    if (mChannel != nullptr) {
        mChannel->unregisterDtoHandler("AR");
    }
    mChannel = newChannel;
    if (mChannel != nullptr) {
        mChannel->registerDtoHandler(
                "AR", [this](const unsigned char *data, size_t len) {
                    try {
                        dto::AudioRxOnTransceivers rxAudio;
                        auto objHdl = msgpack::unpack(reinterpret_cast<const char *>(data), len);
                        objHdl.get().convert(rxAudio);
                        this->rxVoicePacket(rxAudio);
                    } catch (const msgpack::type_error &e) {
                        LOG("atcradiostack", "unable to unpack audio data received: %s", e.what());
                        LOGDUMPHEX("radiosimulation", data, len);
                    }
                });
    }
}



/** Audio enters here from the Codec Compressor before being sent out on to the network.
 *
 *
 *
 *
 */
void ATCRadioStack::processCompressedFrame(std::vector<unsigned char> compressedData)
{
    if (mChannel != nullptr && mChannel->isOpen()) {
        dto::AudioTxOnTransceivers audioOutDto;
        {
            std::lock_guard<std::mutex> radioStateGuard(mRadioStateLock);

            if (!mPtt.load()) {
                audioOutDto.LastPacket = true;
                mLastFramePtt = false;
            } else {
                mLastFramePtt = true;
            }

            //FIXME: audioOutDto.Transceivers.emplace_back(mTxRadio);
        }
        audioOutDto.SequenceCounter = std::atomic_fetch_add<uint32_t>(&mTxSequence, 1);
        audioOutDto.Callsign = mCallsign;
        audioOutDto.Audio = std::move(compressedData);
        mChannel->sendDto(audioOutDto);
    }
}


void ATCRadioStack::setPtt(bool pressed)
{
    mPtt.store(pressed);
}

double ATCRadioStack::getVu() const
{
    return std::max(-40.0, mVuMeter.getAverage());
}

double ATCRadioStack::getPeak() const
{
    return std::max(-40.0, mVuMeter.getMax());
}

bool ATCRadioStack::getTxActive(unsigned int freq)
{
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    if(!mRadioState[freq].tx) return false;
    return mPtt.load();
}
bool ATCRadioStack::getRxActive(unsigned int freq)
{
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    return (mRadioState[freq].mLastRxCount >0);
}

void ATCRadioStack::addFrequency(unsigned int freq, std::vector<dto::Transceiver> transceivers, bool onHeadset)
{
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    mRadioState[freq].Frequency=freq;
    mRadioState[freq].transceivers=transceivers;
    mRadioState[freq].onHeadset=onHeadset;
    mRadioState[freq].tx=false;
    mRadioState[freq].rx=true;
}

void ATCRadioStack::setCallsign(const std::string &newCallsign)
{
    mCallsign = newCallsign;
}

void ATCRadioStack::setGain(unsigned int freq, float gain)
{
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    mRadioState[freq].Gain=gain;
}

void ATCRadioStack::setTx(unsigned int freq, bool tx)
{
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    if (tx==false && mRadioState[freq].rx==false)
    {
        mRadioState.erase(freq);
        //FIXME: Update Transceivers here
        return;
    }
    mRadioState[freq].tx=tx;
}

void ATCRadioStack::setRx(unsigned int freq, bool rx)
{
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    if (rx==false && mRadioState[freq].tx==false)
    {
        mRadioState.erase(freq);
        //FIXME: Update Transceivers here
        return;
    }
    mRadioState[freq].rx=rx;
}

void ATCRadioStack::setOnHeadset(unsigned int freq, bool onHeadset)
{
    std::lock_guard<std::mutex> mRadioStateGuard(mRadioStateLock);
    mRadioState[freq].onHeadset=onHeadset;
}

void ATCRadioStack::reset()
{
    {
        std::lock_guard<std::mutex> ml(mStreamMapLock);
        mIncomingStreams.clear();
    }
    mTxSequence.store(0);
    mPtt.store(false);
    mLastFramePtt = false;
    // reset the voice compression codec state.
    mVoiceSink->reset();
}

void ATCRadioStack::maintainIncomingStreams()
{
    std::lock_guard<std::mutex> ml(mStreamMapLock);
    std::vector<std::string> callsignsToPurge;
    util::monotime_t now = util::monotime_get();
    for (const auto &streamPair: mIncomingStreams) {
        auto idleTime = now - streamPair.second.source->getLastActivityTime();
        if ((now - streamPair.second.source->getLastActivityTime()) > audio::compressedSourceCacheTimeoutMs) {
            callsignsToPurge.emplace_back(streamPair.first);
        }
    }
    for (const auto &callsign: callsignsToPurge) {
        mIncomingStreams.erase(callsign);
    }
    mMaintenanceTimer.enable(maintenanceTimerIntervalMs);
    
}

