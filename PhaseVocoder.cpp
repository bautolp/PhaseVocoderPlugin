#include "PhaseVocoder.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <ctime>
#include <iostream>
#include <bitset>
#include <chrono>
#include <math.h>
#include <fstream>
#include <iomanip>

#define DEFAULT_BUFFER_SIZE 1024 * 1024 * 16 // 16,000,000 samples * 4 byte/sample = 64 MB should be much more than required for any audio
#define FFT_SIZE 4096
#define SEGMENT_SIZE 1024
#define WINDOW_SIZE 128
#define HOP_SIZE 32

PhaseVocoder::PhaseVocoder(uint32_t window_size, uint32_t hop_size, WindowFunctionType window_type)
    : m_window_size(window_size), m_hop_size(hop_size), m_forward_fft(NULL), m_reverse_fft(NULL), 
    m_window_buffer(NULL)
{
    m_buffer_size[0] = 0;
    m_buffer_size[1] = 0;
    complex_in = new dsp::Complex<float>[FFT_SIZE * 2];
    complex_out = new dsp::Complex<float>[FFT_SIZE * 2];
    m_input_buffer[0] = new float[DEFAULT_BUFFER_SIZE];
    m_input_buffer[1] = new float[DEFAULT_BUFFER_SIZE];
    m_output_buffer[0] = new float[DEFAULT_BUFFER_SIZE];
    m_output_buffer[1] = new float[DEFAULT_BUFFER_SIZE];

    int order = -1;
    for (int i = 0; i < 32; i++)
    {
        if ((FFT_SIZE >> i) & 1)
            order = i;
    }
    m_forward_fft = new dsp::FFT(order);
    m_reverse_fft = new dsp::FFT(order);
    m_window_buffer = new float[2*FFT_SIZE + m_window_size];
    m_window_function = new float[2*FFT_SIZE + m_window_size];
    GenerateWindowFunction(window_type);

}

void PhaseVocoder::Setup(uint32_t window_size, uint32_t hop_size, WindowFunctionType window_type)
{
    m_window_size = window_size;
    m_hop_size = hop_size;

    if (m_window_buffer)
        delete[] m_window_buffer;
    m_window_buffer = new float[m_window_size];

    if (m_window_function)
        delete[] m_window_function;
    m_window_function = new float[m_window_size];

    GenerateWindowFunction(window_type);
}

void PhaseVocoder::GenerateWindowFunction(WindowFunctionType window_type)
{
    switch (window_type)
    {
    case WindowFunctionType::Flat:
        for (uint32_t curr_pos = 0; curr_pos < m_window_size; curr_pos++)
        {
            m_window_function[curr_pos] = (float)(m_hop_size / m_window_size);
        }
        break;
    case WindowFunctionType::Hanning:
        for (uint32_t curr_pos = 0; curr_pos < m_window_size; curr_pos++)
        {
            m_window_function[curr_pos] = 0.5*(1 - (float)cos((2 * M_PI * curr_pos) / (m_window_size - 1 + 1e-20)));
        }
        break;
    }
}

PhaseVocoder::~PhaseVocoder()
{
    ProcessBuffer();
    
    if (m_forward_fft)
        delete m_forward_fft;
    if (m_reverse_fft)
        delete m_reverse_fft;
    if (m_window_buffer)
        delete[] m_window_buffer;
    if (m_window_function)
        delete[] m_window_function;
    if (m_input_buffer[0])
        delete[] m_input_buffer[0];
    if (m_input_buffer[1])
        delete[] m_input_buffer[1];
    if (m_output_buffer[0])
        delete[] m_output_buffer[0];
    if (m_output_buffer[1])
        delete[] m_output_buffer[1];
    if (complex_in)
        delete[] complex_in;
    if (complex_out)
        delete[] complex_out;
}

void PhaseVocoder::ApplyWindowFunction(float* input, float* output, uint32_t count)
{
    for (uint32_t window_pos = 0; window_pos < count; window_pos++)
    {
        output[window_pos] = m_window_function[window_pos] * input[window_pos];
    }
}

void PhaseVocoder::ApplyProcessing(float* input, float* intermed, float* output, uint32_t count)
{
    ApplyWindowFunction(input, intermed, count);

    // Perform FFT
    //   Note: There is a perform function which may be good to look into if we want complex parts. It takes in a complex buffer. Not really sure how it all works, 
    //   and we may not need it, but it may be useful at some point.
    uint32_t foo = m_forward_fft->getSize();

    for (int i = 0; i < FFT_SIZE; i++)
    {
        complex_in[i] = (intermed[i], 0.0f);
    }
    m_forward_fft->perform(complex_in, complex_out, false);

    Process();

    // Perform Phase locking (currently does nothing)
    PhaseLock();
    
    // Inverse FFT
    m_forward_fft->perform(complex_out, complex_in, true);

    for (int i = 0; i < FFT_SIZE;i++)
    {
        intermed[i] = complex_out[i].real();
    }

    // Scale buffer back
    ReScaleWindow(intermed, count, (float)FFT_SIZE);

    // Commit data to output buffer, since it is windowed we can just add
    WriteWindow(intermed, output, count);
}

void PhaseVocoder::PhaseLock()
{

}

void PhaseVocoder::ReScaleWindow(float* buffer, uint32_t count, float scale)
{
    for (uint32_t window_pos = 0; window_pos < count; window_pos++)
    {
        buffer[window_pos] = buffer[window_pos] / (float)m_window_size;
    }
}

void PhaseVocoder::WriteWindow(float* input, float* output, uint32_t count)
{
    for (uint32_t window_pos = 0; window_pos < count; window_pos++)
    {
        output[window_pos] += input[window_pos];
    }
}

void PhaseVocoder::DSPProcessing(float* input, float* output, uint32_t buff_size, uint32_t channel)
{
    uint32_t initial_val = m_buffer_size[channel];
    for (m_buffer_size[channel]; m_buffer_size[channel] < (buff_size + initial_val); m_buffer_size[channel]++)
    {
        m_input_buffer[channel][m_buffer_size[channel]] = output[m_buffer_size[channel] - initial_val];
    }
}

std::string PhaseVocoder::LookupSafeWriteLocation()
{
    char *appdata = getenv("APPDATA");
    std::string app_data(appdata);
    return app_data + "\\PhaseVocoder\\";
}

std::string PhaseVocoder::LookupSafeFileName()
{
    std::time_t t = std::time(0);
    std::tm* now = std::localtime(&t);
    return std::to_string(now->tm_year + 1900) + "-" + std::to_string(now->tm_mon + 1) + "-" + std::to_string(now->tm_mday) + ".wav";
}

void PhaseVocoder::ProcessBuffer()
{
    std::string output_file = LookupSafeWriteLocation() + LookupSafeFileName();

    // Zero output buffer before starting
    for (uint32_t i = 0; i < m_buffer_size[0]; i++)
    {
        m_output_buffer[0][i] = 0.0;
    }
    for (uint32_t i = 0; i < m_buffer_size[1]; i++)
    {
        m_output_buffer[1][i] = 0.0;
    }

    // Loop through segments
    for (uint32_t i = 0; i < (m_buffer_size[0] - SEGMENT_SIZE); i += SEGMENT_SIZE)
    {
        ProcessSegment((m_input_buffer[0] + i), (m_output_buffer[0] + i), SEGMENT_SIZE);
    }
    for (uint32_t i = 0; i < (m_buffer_size[1] - SEGMENT_SIZE); i += SEGMENT_SIZE)
    {
        ProcessSegment((m_input_buffer[1] + i), (m_output_buffer[1] + i), SEGMENT_SIZE);
    }

    // If there is leftover data, process it
    if (m_buffer_size[0] % SEGMENT_SIZE)
        ProcessSegment(m_input_buffer[0] + m_buffer_size[0] - (m_buffer_size[0] % SEGMENT_SIZE),
            m_output_buffer[0] + m_buffer_size[0] - (m_buffer_size[0] % SEGMENT_SIZE),
            m_buffer_size[0] % SEGMENT_SIZE);
   if (m_buffer_size[1] % SEGMENT_SIZE)
        ProcessSegment(m_input_buffer[1] + m_buffer_size[1] - (m_buffer_size[1] % SEGMENT_SIZE),
            m_output_buffer[1] + m_buffer_size[1] - (m_buffer_size[1] % SEGMENT_SIZE),
            m_buffer_size[1] % SEGMENT_SIZE);

    // Write buffer to file
    CommitBuffer(output_file);
}

template <typename Word>
std::ostream& write_word(std::ostream& outs, Word value, unsigned size = sizeof(Word))
{
    for (; size; --size, value >>= 8)
        outs.put(static_cast <char> (value & 0xFF));
    return outs;
}

void PhaseVocoder::CommitBuffer(std::string output_file)
{
    static int i = 0;
    i++;
    std::ofstream f("C:\\school\\output_data" + std::to_string(i) + ".wav", std::ios::binary);

    // Write the file headers
    f << "RIFF----WAVEfmt ";            // (chunk size to be filled in later)
    write_word(f, 16, 4);               // no extension data
    write_word(f, 1, 2);                // PCM - integer samples
    write_word(f, 2, 2);                // two channels (stereo file)
    write_word(f, 44100, 4);            // samples per second (Hz)
    write_word(f, (44100*16*2)/8, 4);   // (Sample Rate * BitsPerSample * Channels) / 8
    write_word(f, 4, 2);                // data block size (size of two integer samples, one for each channel, in bytes)
    write_word(f, 16, 2);               // number of bits per sample (use a multiple of 8)

    // Write the data chunk header
    size_t data_chunk_pos = f.tellp();
    f << "data----";  // (chunk size to be filled in later)

    // Write the audio samples
    // (We'll generate a single C4 note with a sine wave, fading from left to right)
    /*constexpr double two_pi = 6.283185307179586476925286766559;
    constexpr double max_amplitude = 32760;  // "volume"

    double hz = 44100;    // samples per second
    double frequency = 261.626;  // middle C
    double seconds = 2.5;      // time

    int N = hz * seconds;  // total number of samples
    for (int n = 0; n < N; n++)
    {
        double amplitude = (double)n / N * max_amplitude;
        double value = sin((two_pi * n * frequency) / hz);
        write_word(f, (int)(amplitude  * value), 2);
        write_word(f, (int)((max_amplitude - amplitude) * value), 2);
    }*/
    for (int i = 0; i < m_buffer_size[0]; i++)
    {
        write_word(f, (int)(m_output_buffer[0][i]), 2);
        write_word(f, (int)(m_output_buffer[1][i]), 2);
    }

    // (We'll need the final file size to fix the chunk sizes above)
    size_t file_length = f.tellp();

    // Fix the data chunk header to contain the data size
    f.seekp(data_chunk_pos + 4);
    write_word(f, file_length - data_chunk_pos + 8);

    // Fix the file header to contain the proper RIFF chunk size, which is (file size - 8) bytes
    f.seekp(0 + 4);
    write_word(f, file_length - 8, 4);
}

void PhaseVocoder::Process()
{

}

void PhaseVocoder::ProcessSegment(float* input_buffer, float * output_buffer, uint32_t segment_size)
{
    uint32_t window_size = m_window_size < segment_size ? m_window_size : segment_size / 8;

    // Too small
    if (window_size < 8 || window_size < m_hop_size)
    {
        for (uint32_t i = 0; i < segment_size; i++)
        {
            output_buffer[i] = input_buffer[i];
        }
        return;
    }

    uint32_t window_start_pos, count, buffer_start_pos;

    // Get left edge (don't start at 0, need at least 1 sample, so start at hop size)
    buffer_start_pos = 0;
    for (uint32_t end_pos = m_hop_size; end_pos < window_size; end_pos += m_hop_size)
    {
        // Put these here for now, will remove later, just here so code is explicit
        count = end_pos;
        window_start_pos = window_size - end_pos;
        ApplyProcessing(input_buffer + buffer_start_pos, windowed_buffer + window_start_pos, output_buffer + buffer_start_pos, count);
    }

    // Get middle
    count = window_size;
    for (uint32_t start_pos = 0; start_pos < (segment_size - window_size); start_pos++)
    {
        // Put these here for now, will remove later, just here so code is explicit
        window_start_pos = start_pos;
        buffer_start_pos = start_pos;
        ApplyProcessing(input_buffer + buffer_start_pos, windowed_buffer + window_start_pos, output_buffer + buffer_start_pos, count);
    }

    // Get right edge
    for (uint32_t start_pos = segment_size - window_size; start_pos < segment_size; start_pos += m_hop_size)
    {
        // Put these here for now, will remove later, just here so code is explicit
        count = segment_size - start_pos;
        window_start_pos = 0;
        buffer_start_pos = start_pos;
        ApplyProcessing(input_buffer + buffer_start_pos, windowed_buffer + window_start_pos, output_buffer + buffer_start_pos, count);
    }
}