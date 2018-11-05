/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

class PhaseVocoder
{
public:
    PhaseVocoder(uint32_t window_size, uint32_t hop_size);
    ~PhaseVocoder();

private:
    void  DSPProcessing(float* input, float* output, uint32_t hop_size, uint32_t buff_size);
    uint32_t m_window_size;
    uint32_t m_hop_size;
    dsp::FFT *m_forward_fft;
    dsp::FFT *m_reverse_fft;
    float *m_window_buffer;
};
