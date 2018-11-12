/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

enum WindowFunctionType
{
    Flat,
    Hanning
};

enum ProcessType
{
    PitchShift,
    Robotization,
    Whisperization
};


/* Textbook robotization
for(int bin = 0; bin < fftTransformSize_; bin++)
 {
 float amplitude=sqrt(
 (fftFrequencyDomain[bin][0] *
 fftFrequencyDomain[bin][0]) +
 (fftFrequencyDomain[bin][1] *
 fftFrequencyDomain[bin][1]));
 // Set the phase of each bin to 0. phase = 0
 // means the signal is entirely positive-real,
 // but the overall amplitude is the same
 fftFrequencyDomain[bin][0] = amplitude;
 fftFrequencyDomain[bin][1] = 0.0;

 Textbook whisperization
  float amplitude=sqrt(
 (fftFrequencyDomain[bin][0] *
 fftFrequencyDomain[bin][0]) +
 (fftFrequencyDomain[bin][1] *
 fftFrequencyDomain[bin][1]));
 // This is how we could exactly reconstruct the signal:
 // float phase = atan2(fftFrequencyDomain[bin][1],
 // fftFrequencyDomain[bin][0]);
 // But instead, this is how we scramble the phase:
 float phase = 2.0 * M_PI * (float)rand() /
 (float)RAND_MAX;
 fftFrequencyDomain[bin][0] = amplitude * cos(phase);
 fftFrequencyDomain[bin][1] = amplitude * sin(phase);
 // FFTs of real signals are conjugate-symmetric. We need
 // to maintain that symmetry to produce a real output,
 // even as we randomize the phase.
 if(bin > 0 && bin < fftTransformSize_ / 2) {
 fftFrequencyDomain[fftTransformSize_ - bin][0] =
 amplitude * cos(phase);
 fftFrequencyDomain[fftTransformSize_ - bin][1] =
 -amplitude * sin(phase);
 }
 }*/

#define DEFAULT_BUFFER_SIZE_ONLINE 1024 * 1024 // 1000,000 samples * 4 byte/sample = 4 MB should be much more than required for any audio
#define FFT_ORDER_ONLINE 9
#define FFT_SIZE_ONLINE (1 << FFT_ORDER_ONLINE)
#define SEGMENT_SIZE_ONLINE 256
#define WINDOW_SIZE_ONLINE 128
#define HOP_SIZE_ONLINE 64
#define SCALING_FACTOR_ONLINE ((FFT_SIZE_ONLINE / WINDOW_SIZE_ONLINE) * (WINDOW_SIZE_ONLINE / (HOP_SIZE_ONLINE*2)))

#define DEFAULT_BUFFER_SIZE_OFFLINE 1024 * 1024 * 8 // 8,000,000 samples * 4 byte/sample = 32 MB should be much more than required for any audio
#define FFT_ORDER_OFFLINE 9
#define FFT_SIZE_OFFLINE (1 << FFT_ORDER_OFFLINE)
#define SEGMENT_SIZE_OFFLINE 256
#define WINDOW_SIZE_OFFLINE 64
#define HOP_SIZE_OFFLINE 32
#define SCALING_FACTOR_OFFLINE ((FFT_SIZE_OFFLINE / SEGMENT_SIZE_OFFLINE) / 2)

#define INTERM_BUFFER_SIZE 8192


class PhaseVocoder
{
public:
    PhaseVocoder(WindowFunctionType window_type);
    ~PhaseVocoder();
    void DSPOffline(float* input, float* output, uint32_t buff_size, uint32_t channel);
    void DSPOnline(float* input, float* output, uint32_t buff_size, uint32_t channel);
    void Setup(uint32_t window_size, uint32_t hop_size, WindowFunctionType window_type);
    void Finish();
private:

    // Allocate output buffer and tmp buffer
    uint32_t m_buffer_size[2] = { 0, 0 };

    uint32_t m_buffer_size_online[2] = { 0, 0 };

    float windowed_buffer[INTERM_BUFFER_SIZE];
    float output_buff[INTERM_BUFFER_SIZE];

    dsp::Complex<float> m_complex_intermed_fw_offline[FFT_SIZE_OFFLINE * 2 + WINDOW_SIZE_OFFLINE];
    dsp::Complex<float> m_complex_intermed_rv_offline[FFT_SIZE_OFFLINE * 2 + WINDOW_SIZE_OFFLINE];
    dsp::Complex<float> m_complex_in_offline[2][DEFAULT_BUFFER_SIZE_OFFLINE];
    dsp::Complex<float> m_complex_out_offline[2][DEFAULT_BUFFER_SIZE_OFFLINE];
    dsp::Complex<float> m_test_buffer[2][DEFAULT_BUFFER_SIZE_OFFLINE];
    uint32_t m_window_size_offline = WINDOW_SIZE_OFFLINE;
    uint32_t m_hop_size_offline = HOP_SIZE_OFFLINE;
    dsp::FFT m_forward_fft_offline;
    dsp::FFT m_reverse_fft_offline;
    float m_window_function_offline[2 * FFT_SIZE_OFFLINE + WINDOW_SIZE_OFFLINE];
    float m_window_buffer_offline[2 * FFT_SIZE_OFFLINE + WINDOW_SIZE_OFFLINE];

    void CommitBufferOffline(std::string output_file);
    void ProcessBufferOffline();
    void ProcessSegmentOffline(dsp::Complex<float>* input_buffer, dsp::Complex<float> * output_buffer, uint32_t segment_size);
    void ApplyProcessingOffline(dsp::Complex<float>* input, dsp::Complex<float>* intermed_fw, dsp::Complex<float>* intermed_rv, dsp::Complex<float>* output, uint32_t count, uint32_t window_start);
    void ApplyWindowFunctionOffline(dsp::Complex<float>* input, dsp::Complex<float>* output, uint32_t count, uint32_t window_start);

    dsp::Complex<float> m_complex_intermed_fw_online[FFT_SIZE_OFFLINE * 2 + WINDOW_SIZE_ONLINE];
    dsp::Complex<float> m_complex_intermed_rv_online[FFT_SIZE_OFFLINE * 2 + WINDOW_SIZE_ONLINE];
    dsp::Complex<float> m_complex_in_online[2][DEFAULT_BUFFER_SIZE_ONLINE];
    dsp::Complex<float> m_complex_out_online[2][DEFAULT_BUFFER_SIZE_ONLINE];
    uint32_t m_window_size_online = WINDOW_SIZE_ONLINE;
    uint32_t m_hop_size_online = HOP_SIZE_ONLINE;
    dsp::FFT m_forward_fft_online;
    dsp::FFT m_reverse_fft_online;
    float m_window_function_online[2 * FFT_SIZE_ONLINE + WINDOW_SIZE_ONLINE];
    float m_window_buffer_online[2 * FFT_SIZE_ONLINE + WINDOW_SIZE_ONLINE];

    void CommitBufferOnline(std::string output_file);
    void ProcessSegmentOnline(dsp::Complex<float>* input_buffer, dsp::Complex<float> * output_buffer, uint32_t segment_size);
    void ApplyProcessingOnline(dsp::Complex<float>* input, dsp::Complex<float>* intermed_fw, dsp::Complex<float>* intermed_rv, dsp::Complex<float>* output, uint32_t count, uint32_t window_start);
    void ApplyWindowFunctionOnline(dsp::Complex<float>* input, dsp::Complex<float>* output, uint32_t count, uint32_t window_start);

    std::string LookupSafeWriteLocation();
    std::string LookupSafeFileName();
    void GenerateWindowFunction(WindowFunctionType window_type);
    void Process(dsp::Complex<float> * fft_data, uint32_t fft_size, ProcessType type);
    void PhaseLock();
    void WriteWindow(dsp::Complex<float>* input, dsp::Complex<float>* output, uint32_t count);
    void Whisperization(dsp::Complex<float> * fft_data, uint32_t fft_size);
    void Robotization(dsp::Complex<float> * fft_data, uint32_t fft_size);
};
