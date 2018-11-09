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

PhaseVocoder::PhaseVocoder(uint32_t window_size, uint32_t hop_size, WindowFunctionType window_type)
    : m_window_size(window_size), m_hop_size(hop_size), m_forward_fft(FFT_ORDER), m_reverse_fft(FFT_ORDER)
{
    GenerateWindowFunction(window_type);
}

void PhaseVocoder::Finish()
{
    if (m_buffer_size[0])
        ProcessBuffer();
}

void PhaseVocoder::Setup(uint32_t window_size, uint32_t hop_size, WindowFunctionType window_type)
{
    /*m_window_size = window_size;
    m_hop_size = hop_size;

    if (m_window_buffer)
        delete[] m_window_buffer;
    m_window_buffer = new float[m_window_size];

    if (m_window_function)
        delete[] m_window_function;
    m_window_function = new float[m_window_size];

    GenerateWindowFunction(window_type);*/
}

void PhaseVocoder::GenerateWindowFunction(WindowFunctionType window_type)
{
    switch (window_type)
    {
    case WindowFunctionType::Flat:
        for (uint32_t curr_pos = 0; curr_pos < m_window_size; curr_pos++)
        {
            m_window_function[curr_pos] = (float)((float)m_hop_size / (float)m_window_size);
        }
        break;
    case WindowFunctionType::Hanning:
        for (uint32_t curr_pos = 0; curr_pos < m_window_size; curr_pos++)
        {
            m_window_function[curr_pos] = (float)0.5*((float)1 - (float)cos((2 * M_PI * curr_pos) / ((float)m_window_size - 1 + 1e-20)));
        }
        break;
    }
}

PhaseVocoder::~PhaseVocoder()
{
}

void PhaseVocoder::ApplyWindowFunction(dsp::Complex<float>* input, dsp::Complex<float>* output, uint32_t count)
{
    for (uint32_t window_pos = 0; window_pos < count; window_pos++)
    {
        output[window_pos] = m_window_function[window_pos] * input[window_pos];
    }
}

void PhaseVocoder::ApplyProcessing( dsp::Complex<float>* input, 
                                    dsp::Complex<float>* intermed_fw, 
                                    dsp::Complex<float>* intermed_rv, 
                                    dsp::Complex<float>* output, 
                                    uint32_t count)
{
    dsp::Complex<float> buff[FFT_SIZE * 2];
    ApplyWindowFunction(input, buff, count);

    for (uint32_t i = count; i < FFT_SIZE * 2; i++)
    {
        buff[i] = dsp::Complex<float>(0.0f, 0.0f);
    }
    m_forward_fft.perform(buff, intermed_fw, false);

    for (uint32_t i = 0; i < FFT_SIZE * 2; i++)
    {
        intermed_fw[i] = dsp::Complex<float>(   intermed_fw[i].real() / ((FFT_SIZE / SEGMENT_SIZE) * (WINDOW_SIZE / HOP_SIZE)), 
                                                intermed_fw[i].imag() / (FFT_SIZE / SEGMENT_SIZE));
    }

    Process();

    // Perform Phase locking (currently does nothing)
    PhaseLock();
    
    // Inverse FFT
    m_reverse_fft.perform(intermed_fw, intermed_rv, true);


    // Scale buffer back
    //ReScaleWindow(intermed, count, (float)FFT_SIZE);

    // Commit data to output buffer, since it is windowed we can just add
    WriteWindow(intermed_rv, output, count);
}

void PhaseVocoder::PhaseLock()
{

}

void PhaseVocoder::ReScaleWindow(float* buffer, uint32_t count, float scale)
{
    for (uint32_t window_pos = 0; window_pos < count; window_pos++)
    {
        buffer[window_pos] = buffer[window_pos] * scale;
    }
}

void PhaseVocoder::WriteWindow(dsp::Complex<float>* input, dsp::Complex<float>* output, uint32_t count)
{
    //float factor = (float)FFT_SIZE / (float)1024;
    for (uint32_t window_pos = 0; window_pos < count; window_pos++)
    {
        output[window_pos] += dsp::Complex<float>(  input[window_pos].real(),
                                                    input[window_pos].imag());
    }
}

void PhaseVocoder::DSPProcessing(float* input, float* output, uint32_t buff_size, uint32_t channel)
{
    uint32_t initial_val = m_buffer_size[channel];
    for (m_buffer_size[channel]; m_buffer_size[channel] < (buff_size + initial_val); m_buffer_size[channel]++)
    {
        m_complex_in[channel][m_buffer_size[channel]] = dsp::Complex<float>(input[m_buffer_size[channel] - initial_val], 0.0f);
        //m_input_buffer[channel][m_buffer_size[channel]] = input[m_buffer_size[channel] - initial_val];
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
        m_complex_out[0][i] = dsp::Complex<float>(0.0f, 0.0f);
    }
    for (uint32_t i = 0; i < m_buffer_size[1]; i++)
    {
        m_complex_out[1][i] = dsp::Complex<float>(0.0f, 0.0f);
    }

    // Loop through segments
    ProcessSegment((m_complex_in[0]), (m_complex_out[0]), m_buffer_size[0]);
    ProcessSegment((m_complex_in[1]), (m_complex_out[1]), m_buffer_size[1]);
    /*
    for (uint32_t i = 0; i < (m_buffer_size[0] - SEGMENT_SIZE); i += SEGMENT_SIZE)
    {
        ProcessSegment((m_complex_in[0] + i), (m_complex_out[0] + i), SEGMENT_SIZE);
    }
    for (uint32_t i = 0; i < (m_buffer_size[1] - SEGMENT_SIZE); i += SEGMENT_SIZE)
    {
        ProcessSegment((m_complex_in[1] + i), (m_complex_out[1] + i), SEGMENT_SIZE);
    }*/

    // If there is leftover data, process it
    /*if (m_buffer_size[0] % SEGMENT_SIZE)
        ProcessSegment(m_complex_in[0] + m_buffer_size[0] - (m_buffer_size[0] % SEGMENT_SIZE),
            m_complex_out[0] + m_buffer_size[0] - (m_buffer_size[0] % SEGMENT_SIZE),
            m_buffer_size[0] % SEGMENT_SIZE);
    if (m_buffer_size[1] % SEGMENT_SIZE)
        ProcessSegment(m_complex_in[1] + m_buffer_size[1] - (m_buffer_size[1] % SEGMENT_SIZE),
            m_complex_out[1] + m_buffer_size[1] - (m_buffer_size[1] % SEGMENT_SIZE),
            m_buffer_size[1] % SEGMENT_SIZE);*/

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
    std::ofstream f("C:\\school\\output_data_foo" + std::to_string(i) + ".wav", std::ios::binary);

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

    for (uint32_t j = 0; j < m_buffer_size[0]; j++)
    {
        write_word(f, (int)(32760.0f * m_complex_out[0][j].real()), 2);
        write_word(f, (int)(32760.0f * m_complex_out[1][j].real()), 2);
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

void PhaseVocoder::ProcessSegment(dsp::Complex<float>* input_buffer, dsp::Complex<float> * output_buffer, uint32_t segment_size)
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

    for (uint32_t i = 0; i < segment_size - (segment_size % WINDOW_SIZE); i += m_hop_size)
    {
        ApplyProcessing(input_buffer + i, m_complex_intermed_fw, m_complex_intermed_rv, output_buffer + i, WINDOW_SIZE);
    }

    uint32_t uncounted_factor = segment_size % WINDOW_SIZE;
    if (uncounted_factor)
    {
        for (uint32_t i = 0; i < segment_size - uncounted_factor; i++)
        {
            input_buffer[segment_size - uncounted_factor + i] = output_buffer[segment_size - uncounted_factor + i];
        }
    }

    /*
    uint32_t window_start_pos, count, buffer_start_pos;

    // Get left edge (don't start at 0, need at least 1 sample, so start at hop size)
    buffer_start_pos = 0;
    for (uint32_t end_pos = m_hop_size; end_pos < window_size; end_pos += m_hop_size)
    {
        // Put these here for now, will remove later, just here so code is explicit
        count = end_pos;
        window_start_pos = window_size - end_pos;
        ApplyProcessing(input_buffer + buffer_start_pos, m_complex_intermed_fw, m_complex_intermed_rv, output_buffer + buffer_start_pos, count);
    }

    // Get middle
    count = window_size;
    for (uint32_t start_pos = 0; start_pos < (segment_size - window_size); start_pos += m_hop_size)
    {
        // Put these here for now, will remove later, just here so code is explicit
        window_start_pos = start_pos;
        buffer_start_pos = start_pos;
        ApplyProcessing(input_buffer + buffer_start_pos, m_complex_intermed_fw, m_complex_intermed_rv, output_buffer + buffer_start_pos, count);
    }

    // Get right edge
    for (uint32_t start_pos = segment_size - window_size; start_pos < segment_size; start_pos += m_hop_size)
    {
        // Put these here for now, will remove later, just here so code is explicit
        count = segment_size - start_pos;
        window_start_pos = 0;
        buffer_start_pos = start_pos;
        ApplyProcessing(input_buffer + buffer_start_pos, m_complex_intermed_fw, m_complex_intermed_rv, output_buffer + buffer_start_pos, count);
    }
    */
}