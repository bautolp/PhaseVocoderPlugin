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

#define DEFAULT_BUFFER_SIZE 1024 * 1024 * 16 // 16,000,000 samples * 4 byte/sample = 64 MB should be much more than required for any audio
#define FFT_ORDER 12
#define FFT_SIZE (1 << FFT_ORDER)
#define SEGMENT_SIZE 1024
#define WINDOW_SIZE 128
#define HOP_SIZE 32
#define SCALING_FACTOR ((FFT_SIZE / WINDOW_SIZE) * (WINDOW_SIZE / (HOP_SIZE / 2)))

class PhaseVocoder
{
public:
    PhaseVocoder(uint32_t window_size, uint32_t hop_size, WindowFunctionType window_type);
    ~PhaseVocoder();
    void DSPProcessing(float* input, float* output, uint32_t buff_size, uint32_t channel);
    void Setup(uint32_t window_size, uint32_t hop_size, WindowFunctionType window_type);
    void Finish();
private:

    // Allocate output buffer and tmp buffer
    uint32_t m_buffer_size[2] = { 0, 0 };

    float windowed_buffer[8192];
    float output_buff[8192];

    dsp::Complex<float> m_complex_intermed_fw[FFT_SIZE * 2 + WINDOW_SIZE];
    dsp::Complex<float> m_complex_intermed_rv[FFT_SIZE * 2 + WINDOW_SIZE];
    dsp::Complex<float> m_complex_in[2][DEFAULT_BUFFER_SIZE];
    dsp::Complex<float> m_complex_out[2][DEFAULT_BUFFER_SIZE];
    uint32_t m_window_size = WINDOW_SIZE;
    uint32_t m_hop_size = HOP_SIZE;
    dsp::FFT m_forward_fft;
    dsp::FFT m_reverse_fft;
    float m_window_function[2 * FFT_SIZE + WINDOW_SIZE];
    float m_window_buffer[2 * FFT_SIZE + WINDOW_SIZE];

    void CommitBuffer(std::string output_file);
    void ProcessBuffer();
    void ProcessSegment(dsp::Complex<float>* input_buffer, dsp::Complex<float> * output_buffer, uint32_t segment_size);
    std::string LookupSafeWriteLocation();
    std::string LookupSafeFileName();
    void GenerateWindowFunction(WindowFunctionType window_type);
    void ApplyWindowFunction(dsp::Complex<float>* input, dsp::Complex<float>* output, uint32_t count);
    void ApplyProcessing(dsp::Complex<float>* input, dsp::Complex<float>* intermed_fw, dsp::Complex<float>* intermed_rv, dsp::Complex<float>* output, uint32_t count);
    void Process();
    void PhaseLock();
    void ReScaleWindow(float* buffer, uint32_t count, float scale);
    void WriteWindow(dsp::Complex<float>* input, dsp::Complex<float>* output, uint32_t count);


};
