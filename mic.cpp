// gcc audio_nn.c -o audio_nn -lopenal -lfann -lm -lstdc++
#include <AL/al.h>
#include <AL/alc.h>
#include <iostream>
#include <list>
#include <fann.h>
#include <unistd.h>  // For usleep
#include <cmath>

using std::list;

// ========== FIXED CONSTANTS ==========
#define CAP_SIZE 2048           // Actual audio buffer size
#define FREQ 22050              // Sample rate (half of 44100)
#define NUM_BUFFERS 16          // Number of OpenAL buffers
#define TRAINING_ITERATIONS 128 // Training limit

// Neural network constants - MUST MATCH CAP_SIZE
const unsigned int num_input = CAP_SIZE;
const unsigned int num_output = CAP_SIZE;
const unsigned int num_layers = 3;
const unsigned int num_neurons_hidden = 256;
const float desired_error = 0.01f;
const unsigned int max_epochs = 1;

// Global neural network
struct fann *ann;

// ========== FIXED PROCESSING FUNCTION ==========
void processBuffer(short* buffer, int len) {
    if (len != CAP_SIZE) {
        std::cerr << "ERROR: Buffer size mismatch!" << std::endl;
        return;
    }
    
    // Normalize audio to [-1, 1] for FANN
    fann_type input[CAP_SIZE];
    for (int i = 0; i < CAP_SIZE; i++) {
        input[i] = (fann_type)buffer[i] / 32768.0f;
    }
    
    // Train with same input/output (autoencoder)
    fann_train(ann, input, input);
}

// ========== FIXED RUN INFERENCE FUNCTION ==========
void runInference(short* buffer, int len) {
    if (len != CAP_SIZE) {
        std::cerr << "ERROR: Buffer size mismatch in inference!" << std::endl;
        return;
    }
    
    // Normalize input
    fann_type input[CAP_SIZE];
    for (int i = 0; i < CAP_SIZE; i++) {
        input[i] = (fann_type)buffer[i] / 32768.0f;
    }
    
    // Run neural network
    fann_type* output = fann_run(ann, input);
    
    // Denormalize back to 16-bit audio
    for (int i = 0; i < CAP_SIZE; i++) {
        float sample = output[i] * 32768.0f;
        
        // Clip to 16-bit range
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        
        buffer[i] = (short)sample;
    }
}

// ========== MAIN PROGRAM ==========
int main(int argC, char* argV[]) {
    std::cout << "=== Neural Network Audio Processor ===\n";
    
    // ========== 1. INITIALIZE NEURAL NETWORK ==========
    std::cout << "Creating neural network...\n";
    ann = fann_create_standard(num_layers, num_input, num_neurons_hidden, num_output);
    if (!ann) {
        std::cerr << "ERROR: Failed to create neural network!\n";
        return 1;
    }
    
    fann_set_activation_function_hidden(ann, FANN_SIGMOID_SYMMETRIC);
    fann_set_activation_function_output(ann, FANN_SIGMOID_SYMMETRIC);
    fann_set_training_algorithm(ann, FANN_TRAIN_INCREMENTAL);
    fann_set_learning_rate(ann, 0.1f);
    
    // ========== 2. INITIALIZE OPENAL ==========
    std::cout << "Initializing OpenAL...\n";
    
    // Open playback device
    ALCdevice* audioDevice = alcOpenDevice(NULL);
    if (!audioDevice) {
        std::cerr << "ERROR: Failed to open audio device!\n";
        fann_destroy(ann);
        return 1;
    }
    
    // Create context
    ALCcontext* audioContext = alcCreateContext(audioDevice, NULL);
    alcMakeContextCurrent(audioContext);
    
    // Open capture device
    ALCdevice* inputDevice = alcCaptureOpenDevice(NULL, FREQ, 
                                                 AL_FORMAT_MONO16, FREQ/2);
    if (!inputDevice) {
        std::cerr << "ERROR: Failed to open capture device!\n";
        alcDestroyContext(audioContext);
        alcCloseDevice(audioDevice);
        fann_destroy(ann);
        return 1;
    }
    
    // ========== 3. CREATE BUFFERS AND SOURCE ==========
    ALuint buffers[NUM_BUFFERS];
    ALuint source;
    list<ALuint> bufferQueue;
    
    // Generate buffers
    alGenBuffers(NUM_BUFFERS, buffers);
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        bufferQueue.push_back(buffers[i]);
    }
    
    // Generate source
    alGenSources(1, &source);
    alSourcef(source, AL_PITCH, 1.0f);
    alSourcef(source, AL_GAIN, 1.0f);
    alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    alSourcei(source, AL_LOOPING, AL_FALSE);
    
    // ========== 4. AUDIO PROCESSING LOOP ==========
    std::cout << "Starting audio processing...\n";
    std::cout << "Training for " << TRAINING_ITERATIONS << " iterations...\n";
    
    alcCaptureStart(inputDevice);
    
    short audioBuffer[CAP_SIZE];  // FIXED: Correct size
    int iterations = 0;
    bool done = false;
    ALint samplesIn = 0;
    ALint availBuffers = 0;
    ALuint buffHolder[NUM_BUFFERS];
    
    while (!done) {
        // ========== A. RECOVER PROCESSED BUFFERS ==========
        alGetSourcei(source, AL_BUFFERS_PROCESSED, &availBuffers);
        if (availBuffers > 0) {
            alSourceUnqueueBuffers(source, availBuffers, buffHolder);
            for (int i = 0; i < availBuffers; ++i) {
                bufferQueue.push_back(buffHolder[i]);
            }
        }
        
        // ========== B. CAPTURE NEW AUDIO ==========
        alcGetIntegerv(inputDevice, ALC_CAPTURE_SAMPLES, 1, &samplesIn);
        if (samplesIn > CAP_SIZE) {
            // Capture audio
            alcCaptureSamples(inputDevice, audioBuffer, CAP_SIZE);
            
            if (!bufferQueue.empty()) {
                ALuint currentBuffer = bufferQueue.front();
                bufferQueue.pop_front();
                

                    processBuffer(audioBuffer, CAP_SIZE);
              runInference(audioBuffer, CAP_SIZE);
                // ========== D. QUEUE PROCESSED AUDIO ==========
                alBufferData(currentBuffer, AL_FORMAT_MONO16, 
                           audioBuffer, CAP_SIZE * sizeof(short), FREQ);
                alSourceQueueBuffers(source, 1, &currentBuffer);
                
                // Restart source if needed
                ALint state;
                alGetSourcei(source, AL_SOURCE_STATE, &state);
                if (state != AL_PLAYING) {
                    alSourcePlay(source);
                }
            }
        }
        
        // Small delay to prevent CPU hogging
        usleep(1000);
    }
    
    // ========== 5. CLEANUP ==========
    std::cout << "\nCleaning up...\n";
    
    // Stop capture
    alcCaptureStop(inputDevice);
    alcCaptureCloseDevice(inputDevice);
    
    // Stop source
    alSourceStop(source);
    alDeleteSources(1, &source);
    alDeleteBuffers(NUM_BUFFERS, buffers);
    
    // Destroy context and close device
    alcMakeContextCurrent(NULL);
    alcDestroyContext(audioContext);
    alcCloseDevice(audioDevice);
    
    // Destroy neural network
    fann_destroy(ann);
    
    std::cout << "Program exited successfully.\n";
    return 0;
}
