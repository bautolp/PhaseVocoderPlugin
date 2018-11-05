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
    void DSPProcessing(float* input, float* output, uint32_t hop_size, uint32_t buff_size);

private:

    void GenerateWindowFunction(WindowFunctionType window_type);
    void ApplyWindowFunction(float* buffer);
    void ApplyProcessing();
    void PhaseLock();
    void ReScale(float* buffer, float* scale);

    uint32_t m_window_size;
    uint32_t m_hop_size;
    dsp::FFT *m_forward_fft;
    dsp::FFT *m_reverse_fft;
    float *m_window_buffer;
    float *m_window_function;
};
