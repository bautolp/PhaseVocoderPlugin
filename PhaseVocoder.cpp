#include "PhaseVocoder.h"

PhaseVocoder::PhaseVocoder(uint32_t window_size, uint32_t hop_size)
    : m_window_size(window_size), m_hop_size(hop_size), m_forward_fft(NULL), m_reverse_fft(NULL), m_window_buffer(NULL)
{
    //while(m_window_size >> 2)
    int order = -1;
    for (int i = 0; i < 32; i++)
    {
        /* If current bit is set */
        if ((m_window_size >> i) & 1)
            order = i;
    }
    m_forward_fft = new dsp::FFT(order);
    m_reverse_fft = new dsp::FFT(order);
    m_window_buffer = new float[m_window_size];
}

PhaseVocoder::~PhaseVocoder()
{
    if (m_forward_fft)
        delete m_forward_fft;
    if (m_reverse_fft)
        delete m_reverse_fft;
    if (m_window_buffer)
        delete[] m_window_buffer;
}

void PhaseVocoder::DSPProcessing(float* input, float* output, uint32_t hop_size, uint32_t buff_size)
{
    
}