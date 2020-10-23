//
//  ATCInputMixer.h
//  afv_native
//
//  Created by Mike Evans on 10/22/20.
//

#ifndef ATCInputMixer_h
#define ATCInputMixer_h

#include "afv-native/audio/ISampleSink.h"
#include <map>


namespace afv_native {
    namespace afv {
    
    
    /** InputInPort - Routes Audio
     *
     *
     *
     */
    
    class InputInPort :
    public audio::ISampleSink {
        InputInPort();
        void putAudioFrame(const audio::SampleType *bufferIn) override;
        
        
    };
    
    class InputOutPort {
    public:
        InputOutPort();
    protected:
        std::shared_ptr<audio::ISampleSink> output;
        
    };
    
    
    
    /**   ATCInputMixer - Microphone to Network
     *
     *    Routes Audio from the Microphone(s) and eventually out onto the network.. this can get complicated if G/G Calls/Monitoring/etc are in progress.
     *
     *      Note: Mixer ports are NOT self routing by default.. You need to connect a port to itself if you want it to be a bus.
     *
     */
    
    class ATCInputMixer {
        
    public:
        
        
        ATCInputMixer();
        virtual ~ATCInputMixer();

        /** attachInputDevice Returns an ISampleSink object to attach to the AudioDevice for input, connected to the mixer at the specified port
         *
         *  @return ISampleSink used to pass into the Audio Device as its sink
         *  @param port Mixer port to connect this input device to
         */
        audio::ISampleSink attachInputDevice(unsigned int port);
        
        
        /** attachOutput connects a port to the specified Sink
         *
         *  @param port Mixer Port to connect to
         *  @param sink Sink that will receive all audio flowing out this port
         *
         */
        
        void attachOutput(unsigned int port, std::shared_ptr<audio::ISampleSink> sink);
        
        
        /** makeMixerConnection connect two ports
         *
         *  @param srcport Port where the audio originates
         *  @param dstport Port where the audio is destined
         *  @param connect True to connect, False to Disconnect
         *
         *  */
        
        void makeMixerConnection(unsigned int srcport, unsigned int dstport, bool connect);
        
        
        
        bool hasMixerConnection(unsigned int srcport, unsigned int dstport);
        
        
    protected:
        std::map<unsigned int,InputInPort> mInputs;
        
        
    };
    
    
    }
}


#endif /* ATCInputMixer_h */
