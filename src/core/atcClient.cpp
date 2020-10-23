//
//  atcClient.cpp
//  afv_native
//
//  Created by Mike Evans on 10/18/20.
//

#include "afv-native/atcClient.h"


#include <memory>
#include <functional>

#include "afv-native/Log.h"
#include "afv-native/afv/params.h"
#include "afv-native/afv/VoiceSession.h"

using namespace afv_native;

ATCClient::ATCClient(
        struct event_base *evBase,
        const std::string &resourceBasePath,
        unsigned int numRadios,
        const std::string &clientName,
        std::string baseUrl):
        mFxRes(std::make_shared<afv::EffectResources>(resourceBasePath)),
        mEvBase(evBase),
        mTransferManager(mEvBase),
        mAPISession(mEvBase, mTransferManager, std::move(baseUrl)),
        mVoiceSession(mAPISession),
        mRadioSim(std::make_shared<afv::RadioSimulation>(mEvBase, mFxRes, &mVoiceSession.getUDPChannel(), numRadios)),
        mAudioDevice(),
        mSpeakerDevice(),
        mClientLatitude(0.0),
        mClientLongitude(0.0),
        mClientAltitudeMSLM(100.0),
        mClientAltitudeGLM(100.0),
        mRadioState(2),
        mCallsign(),
        mTxUpdatePending(false),
        mWantPtt(false),
        mPtt(false),
        mTransceiverUpdateTimer(mEvBase, std::bind(&ATCClient::sendTransceiverUpdate, this)),
        mClientName(clientName),
        mAudioApi(0),
        mAudioInputDeviceName(),
        mAudioOutputDeviceName(),
        ClientEventCallback()
{
    mAPISession.StateCallback.addCallback(this, std::bind(&ATCClient::sessionStateCallback, this, std::placeholders::_1));
    mAPISession.AliasUpdateCallback.addCallback(this, std::bind(&ATCClient::aliasUpdateCallback, this));
    mAPISession.StationTransceiversUpdateCallback.addCallback(this, std::bind(&ATCClient::stationTransceiversUpdateCallback, this));
    mVoiceSession.StateCallback.addCallback(this, std::bind(&ATCClient::voiceStateCallback, this, std::placeholders::_1));
    // forcibly synchronise the RadioSim state.
    mRadioSim->setTxRadio(0);
    for (size_t i = 0; i < mRadioState.size(); i++) {
        mRadioSim->setFrequency(i, mRadioState[i].mNextFreq);
    }
}

ATCClient::~ATCClient()
{
    mVoiceSession.StateCallback.removeCallback(this);
    mAPISession.StateCallback.removeCallback(this);
    mAPISession.AliasUpdateCallback.removeCallback(this);

    // disconnect the radiosim from the UDP channel so if it's held open by the
    // audio device, it doesn't crash the client.
    mRadioSim->setPtt(false);
    mRadioSim->setUDPChannel(nullptr);
}

void ATCClient::setClientPosition(double lat, double lon, double amslm, double aglm)
{
    mClientLatitude = lat;
    mClientLongitude = lon;
    mClientAltitudeMSLM = amslm;
    mClientAltitudeGLM = aglm;
}

bool ATCClient::connect()
{
    if (!isAPIConnected()) {
        if (mAPISession.getState() != afv::APISessionState::Disconnected) {
            return false;
        }
        mAPISession.Connect();
    } else {
        mVoiceSession.Connect();
    }
    return true;
}

void ATCClient::disconnect()
{
    // voicesession must come first.
    if (isVoiceConnected()) {
        mVoiceSession.Disconnect(true);
    } else {
        mAPISession.Disconnect();
    }
}

void ATCClient::setCredentials(const std::string &username, const std::string &password)
{
    if (mAPISession.getState() != afv::APISessionState::Disconnected) {
        return;
    }
    mAPISession.setUsername(username);
    mAPISession.setPassword(password);
}

void ATCClient::setCallsign(std::string callsign)
{
    if (isVoiceConnected()) {
        return;
    }
    mVoiceSession.setCallsign(callsign);
    mRadioSim->setCallsign(callsign);
    mCallsign = std::move(callsign);
}

void ATCClient::voiceStateCallback(afv::VoiceSessionState state)
{
    afv::VoiceSessionError voiceError;
    int channelErrno;

    switch (state) {
    case afv::VoiceSessionState::Connected:
        LOG("afv::ATCClient", "Voice Session Connected");
        // if we have a valid mAudioDevice, then do not attempt to restart it.  bad things will happen.
        if (!mAudioDevice) {
            startAudio();
        }
        queueTransceiverUpdate();
        ClientEventCallback.invokeAll(ClientEventType::VoiceServerConnected, nullptr);
        break;
    case afv::VoiceSessionState::Disconnected:
        LOG("afv::ATCClient", "Voice Session Disconnected");
        stopAudio();
        stopTransceiverUpdate();
        // bring down the API session too.
        mAPISession.Disconnect();
        mRadioSim->reset();
        ClientEventCallback.invokeAll(ClientEventType::VoiceServerDisconnected, nullptr);
        break;
    case afv::VoiceSessionState::Error:
        LOG("afv::ATCClient", "got error from voice session");
        stopAudio();
        stopTransceiverUpdate();
        // bring down the API session too.
        mAPISession.Disconnect();
        mRadioSim->reset();
        voiceError = mVoiceSession.getLastError();
        if (voiceError == afv::VoiceSessionError::UDPChannelError) {
            channelErrno = mVoiceSession.getUDPChannel().getLastErrno();
            ClientEventCallback.invokeAll(ClientEventType::VoiceServerChannelError, &channelErrno);
        } else {
            ClientEventCallback.invokeAll(ClientEventType::VoiceServerError, &voiceError);
        }
        break;
    }
}

void ATCClient::sessionStateCallback(afv::APISessionState state)
{
    afv::APISessionError sessionError;
    switch (state) {
    case afv::APISessionState::Reconnecting:
        LOG("afv_native::ATCClient", "Reconnecting API Session");
        break;
    case afv::APISessionState::Running:
        LOG("afv_native::ATCClient", "Connected to AFV API Server");
        if (!isVoiceConnected()) {
            mVoiceSession.setCallsign(mCallsign);
            mVoiceSession.Connect();
            mAPISession.updateStationAliases();
        }
        ClientEventCallback.invokeAll(ClientEventType::APIServerConnected, nullptr);
        break;
    case afv::APISessionState::Disconnected:
        LOG("afv_native::ATCClient", "Disconnected from AFV API Server.  Terminating sessions");
        // because we only ever commence a normal API Session teardown from a voicesession hook,
        // we don't need to call into voiceSession in this case only.
        ClientEventCallback.invokeAll(ClientEventType::APIServerDisconnected, nullptr);
        break;
    case afv::APISessionState::Error:
        LOG("afv_native::ATCClient", "Got error from AFV API Server.  Disconnecting session");
        sessionError = mAPISession.getLastError();
        ClientEventCallback.invokeAll(ClientEventType::APIServerError, &sessionError);
        break;
    default:
        // ignore the other transitions.
        break;
    }
}

void ATCClient::startAudio()
{
    if (!mAudioDevice) {
        LOG("afv::ATCClient", "Initialising Headset Audio...");
        mAudioDevice = audio::AudioDevice::makeDevice(
                mClientName,
                mAudioOutputDeviceName,
                mAudioInputDeviceName,
                mAudioApi);
    } else {
        LOG("afv::ATCClient", "Tried to recreate Headset audio device...");
    }
    mAudioDevice->setSink(mRadioSim);
    mAudioDevice->setSource(mRadioSim);
    if (!mAudioDevice->open()) {
        LOG("afv::ATCClient", "Unable to open Headset audio device.");
        stopAudio();
        ClientEventCallback.invokeAll(ClientEventType::AudioError, nullptr);
    };
    
    
    
    
    
    
    
}

void ATCClient::stopAudio()
{
    if (mAudioDevice) {
        mAudioDevice->close();
        mAudioDevice.reset();
    }
    if (mSpeakerDevice) {
        mSpeakerDevice->close();
        mSpeakerDevice.reset();
    }
}

std::vector<afv::dto::Transceiver> ATCClient::makeTransceiverDto()
{
    std::vector<afv::dto::Transceiver> retSet;
    for (unsigned i = 0; i < mRadioState.size(); i++) {
        retSet.emplace_back(
                i, mRadioState[i].mNextFreq, mClientLatitude,
                mClientLongitude, mClientAltitudeMSLM, mClientAltitudeGLM);
    }
    return std::move(retSet);
}

void ATCClient::sendTransceiverUpdate()
{
    mTransceiverUpdateTimer.disable();
    if (!isAPIConnected() || !isVoiceConnected()) {
        return;
    }
    auto transceiverDto = makeTransceiverDto();
    mTxUpdatePending = true;

    /* ok - magic!
     *
     * so, in order to ensure that we flip the radio states to the CORRECT ONE
     * when the callback fires, we copy capture the update message itself (which is
     * all value copies) and use that to do the internal state update.
    */
    mVoiceSession.postTransceiverUpdate(
            transceiverDto,
            [this, transceiverDto](http::Request *r, bool success) {
                if (success && r->getStatusCode() == 200) {
                    for (unsigned i = 0; i < this->mRadioState.size(); i++) {
                        this->mRadioState[i].mCurrentFreq = transceiverDto[i].Frequency;
                    }
                    this->mTxUpdatePending = false;
                    this->unguardPtt();
                }
            });
    mTransceiverUpdateTimer.enable(afv::afvTransceiverUpdateIntervalMs);
}

void ATCClient::queueTransceiverUpdate()
{
    mTransceiverUpdateTimer.disable();
    if (!isAPIConnected() || !isVoiceConnected()) {
        return;
    }
    mTransceiverUpdateTimer.enable(0);
}


void ATCClient::unguardPtt()
{
    if (mWantPtt && !mPtt) {
        LOG("ATCClient", "PTT was guarded - checking.");
        if (!areTransceiversSynced()) {
            LOG("Client", "Freqs still unsync'd.  Restarting update.");
            queueTransceiverUpdate();
            return;
        }
        LOG("ATCClient", "Freqs in sync - allowing PTT now.");
        mPtt = true;
        mRadioSim->setPtt(true);
        ClientEventCallback.invokeAll(ClientEventType::PttOpen, nullptr);
    }
}

void ATCClient::setPtt(bool pttState)
{
    if (pttState) {
        mWantPtt = true;
        // if we're setting the Ptt, we have to check a few things.
        // if we're still pending an update, and the radios are out of step, guard the Ptt.
        if (!areTransceiversSynced() || mTxUpdatePending) {
            if (!mTxUpdatePending) {
                LOG("ATCClient", "Wanted to Open PTT mid-update - guarding");
                queueTransceiverUpdate();
            }
            return;
        }
    } else {
        mWantPtt = false;
    }
    if (mWantPtt == mPtt) {
        return;
    }
    mPtt = mWantPtt;
    mRadioSim->setPtt(mPtt);
    if (mPtt) {
        LOG("Client", "Opened PTT");
        ClientEventCallback.invokeAll(ClientEventType::PttOpen, nullptr);
    } else if (!mWantPtt) {
        LOG("Client", "Closed PTT");
        ClientEventCallback.invokeAll(ClientEventType::PttClosed, nullptr);
    }
}

bool ATCClient::areTransceiversSynced() const
{
    for (const auto &iter: mRadioState) {
        if (iter.mNextFreq != iter.mCurrentFreq) {
            return false;
        }
    }
    return true;
}

void ATCClient::setAudioInputDevice(std::string inputDevice)
{
    mAudioInputDeviceName = inputDevice;
}

void ATCClient::setAudioOutputDevice(std::string outputDevice)
{
    mAudioOutputDeviceName = outputDevice;
}

void ATCClient::setSpeakerOutputDevice(std::string outputDevice)
{
    mAudioSpeakerDeviceName = outputDevice;
}

bool ATCClient::isAPIConnected() const
{
    auto sState = mAPISession.getState();
    return sState == afv::APISessionState::Running || sState == afv::APISessionState::Reconnecting;
}

bool ATCClient::isVoiceConnected() const
{
    return mVoiceSession.isConnected();
}

void ATCClient::setBaseUrl(std::string newUrl)
{
    mAPISession.setBaseUrl(std::move(newUrl));
}

void ATCClient::stopTransceiverUpdate()
{
    mTransceiverUpdateTimer.disable();
}

void ATCClient::setAudioApi(audio::AudioDevice::Api api)
{
    mAudioApi = api;
}

void ATCClient::setRadioGain(unsigned int radioNum, float gain)
{
    mRadioSim->setGain(radioNum, gain);
}

bool ATCClient::getEnableInputFilters() const
{
    return mRadioSim->getEnableInputFilters();
}

void ATCClient::setEnableInputFilters(bool enableInputFilters)
{
    mRadioSim->setEnableInputFilters(enableInputFilters);
}

double ATCClient::getInputPeak() const
{
    if (mRadioSim) {
        return mRadioSim->getPeak();
    }
    return -INFINITY;
}

double ATCClient::getInputVu() const
{
    if (mRadioSim) {
        return mRadioSim->getVu();
    }
    return -INFINITY;
}

void ATCClient::setEnableOutputEffects(bool enableEffects)
{
    mRadioSim->setEnableOutputEffects(enableEffects);
}

void ATCClient::aliasUpdateCallback()
{
    ClientEventCallback.invokeAll(ClientEventType::StationAliasesUpdated, nullptr);
}

void ATCClient::stationTransceiversUpdateCallback()
{
    ClientEventCallback.invokeAll(ClientEventType::StationTransceiversUpdated, nullptr);
    
}

std::map<std::string, std::vector<afv::dto::StationTransceiver>> ATCClient::getStationTransceivers() const
{
    return std::move(mAPISession.getStationTransceivers());   
}


std::vector<afv::dto::Station> ATCClient::getStationAliases() const
{
    return std::move(mAPISession.getStationAliases());
}

void ATCClient::logAudioStatistics() {
    if (mAudioDevice) {
        LOG("ATCClient", "Output Buffer Underflows: %d", mAudioDevice->OutputUnderflows.load());
        LOG("ATCClient", "Input Buffer Overflows: %d", mAudioDevice->InputOverflows.load());
    }
}

std::shared_ptr<const afv::RadioSimulation> ATCClient::getRadioSimulation() const {
    return mRadioSim;
}

std::shared_ptr<const audio::AudioDevice> ATCClient::getAudioDevice() const {
    return mAudioDevice;
}

bool ATCClient::getRxActive(unsigned int radioNumber) {
    if (mRadioSim) {
        return mRadioSim->getRxActive(radioNumber);
    }
    return false;
}

bool ATCClient::getTxActive(unsigned int radioNumber) {
    if (mRadioSim) {
        return mRadioSim->getTxActive(radioNumber);
    }
    return false;
}

void ATCClient::requestStationTransceivers(std::string inStation)
{
    mAPISession.requestStationTransceivers(inStation);
}


