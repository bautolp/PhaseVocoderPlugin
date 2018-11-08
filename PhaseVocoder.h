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

class PhaseVocoder
{
public:
    PhaseVocoder(uint32_t window_size, uint32_t hop_size, WindowFunctionType window_type);
    ~PhaseVocoder();
    void DSPProcessing(float* input, float* output, uint32_t buff_size, uint32_t channel);
    void Setup(uint32_t window_size, uint32_t hop_size, WindowFunctionType window_type);

private:

    // Allocate output buffer and tmp buffer
    uint32_t m_buffer_size[2];
    float * m_input_buffer[2];
    float * m_output_buffer[2];

    float windowed_buffer[8192];
    float output_buff[8192];

    void CommitBuffer(std::string output_file);
    void ProcessBuffer();
    void ProcessSegment(float* input_buffer, float* output_buffer, uint32_t segment_size);
    std::string LookupSafeWriteLocation();
    std::string LookupSafeFileName();
    void GenerateWindowFunction(WindowFunctionType window_type);
    void ApplyWindowFunction(float* input, float* output, uint32_t count);
    void ApplyProcessing(float* input, float* intermed, float* output, uint32_t count);
    void Process();
    void PhaseLock();
    void ReScaleWindow(float* buffer, uint32_t count, float scale);
    void WriteWindow(float* input, float* output, uint32_t count);

    dsp::Complex<float>* complex_in;
    dsp::Complex<float>* complex_out;
    uint32_t m_window_size;
    uint32_t m_hop_size;
    dsp::FFT *m_forward_fft;
    dsp::FFT *m_reverse_fft;
    float *m_window_function;
    float* m_window_buffer;
};
