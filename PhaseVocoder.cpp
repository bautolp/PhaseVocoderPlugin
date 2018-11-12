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

PhaseVocoder::PhaseVocoder(WindowFunctionType window_type) :
        m_window_size_offline(WINDOW_SIZE_OFFLINE), m_hop_size_offline(HOP_SIZE_OFFLINE), 
        m_forward_fft_offline(FFT_ORDER_OFFLINE), m_reverse_fft_offline(FFT_ORDER_OFFLINE),
        m_window_size_online(WINDOW_SIZE_ONLINE), m_hop_size_online(HOP_SIZE_ONLINE),
        m_forward_fft_online(FFT_ORDER_ONLINE), m_reverse_fft_online(FFT_ORDER_ONLINE)
{
    GenerateWindowFunction(window_type);
    for (uint32_t i = 0; i < DEFAULT_BUFFER_SIZE_OFFLINE; i++)
    {
        m_test_buffer[0][i] = dsp::Complex<float>(0.0f, 0.0f);
        m_test_buffer[1][i] = dsp::Complex<float>(0.0f, 0.0f);
    }
}

void PhaseVocoder::Finish()
{
    if (m_buffer_size[0])
        ProcessBufferOffline();
}

void PhaseVocoder::Setup(uint32_t window_size, uint32_t hop_size, WindowFunctionType window_type)
{
    (void)window_size;
    (void)hop_size;
    (void)window_type;
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
        for (uint32_t curr_pos = 0; curr_pos < m_window_size_offline; curr_pos++)
        {
            m_window_function_offline[curr_pos] = (float)((float)m_hop_size_offline / (float)m_window_size_offline);
        }
        for (uint32_t curr_pos = 0; curr_pos < m_window_size_online; curr_pos++)
        {
            m_window_function_online[curr_pos] = (float)((float)m_hop_size_online / (float)m_window_size_online);
        }
        break;
    case WindowFunctionType::Hanning:
        for (uint32_t curr_pos = 0; curr_pos < m_window_size_offline; curr_pos++)
        {
            m_window_function_offline[curr_pos] = (float)0.5*((float)1 - (float)cos((2 * M_PI * curr_pos) / ((float)m_window_size_offline - 1 + 1e-20)));
        }
        for (uint32_t curr_pos = 0; curr_pos < m_window_size_online; curr_pos++)
        {
            m_window_function_online[curr_pos] = (float)0.5*((float)1 - (float)cos((2 * M_PI * curr_pos) / ((float)m_window_size_online - 1 + 1e-20)));
        }
        break;
    }
}

PhaseVocoder::~PhaseVocoder()
{
}

void PhaseVocoder::ApplyWindowFunctionOffline(dsp::Complex<float>* input, dsp::Complex<float>* output, uint32_t count, uint32_t window_start)
{
    for (uint32_t window_pos = 0; window_pos < count; window_pos++)
    {
        output[window_pos] = m_window_function_offline[window_start + window_pos] * input[window_pos];
    }
}

void PhaseVocoder::ApplyWindowFunctionOnline(dsp::Complex<float>* input, dsp::Complex<float>* output, uint32_t count, uint32_t window_start)
{
    for (uint32_t window_pos = 0; window_pos < count; window_pos++)
    {
        output[window_pos] = m_window_function_online[window_start + window_pos] * input[window_pos];
    }
}

void PhaseVocoder::ApplyProcessingOffline(  dsp::Complex<float>* input,
                                            dsp::Complex<float>* intermed_fw, 
                                            dsp::Complex<float>* intermed_rv, 
                                            dsp::Complex<float>* output, 
                                            uint32_t count,
                                            uint32_t window_start)
{
    dsp::Complex<float> buff[FFT_SIZE_OFFLINE];
    ApplyWindowFunctionOffline(input, buff, count, window_start);

    for (uint32_t i = count; i < FFT_SIZE_OFFLINE; i++)
    {
        buff[i] = dsp::Complex<float>(0.0f, 0.0f);
    }
    m_forward_fft_offline.perform(buff, intermed_fw, false);

    for (uint32_t i = 0; i < FFT_SIZE_OFFLINE; i++)
    {
        intermed_fw[i] = dsp::Complex<float>(   intermed_fw[i].real() / (SCALING_FACTOR_OFFLINE),
                                                intermed_fw[i].imag() / (SCALING_FACTOR_OFFLINE));
    }

    Process();

    // Perform Phase locking (currently does nothing)
    PhaseLock();
    
    // Inverse FFT
    m_reverse_fft_offline.perform(intermed_fw, intermed_rv, true);


    // Scale buffer back
    //ReScaleWindow(intermed, count, (float)FFT_SIZE);

    // Commit data to output buffer, since it is windowed we can just add
    WriteWindow(intermed_rv, output, count);
}

void PhaseVocoder::PhaseLock()
{

}

void PhaseVocoder::WriteWindow(dsp::Complex<float>* input, dsp::Complex<float>* output, uint32_t count)
{
    for (uint32_t window_pos = 0; window_pos < count; window_pos++)
    {
        output[window_pos] += dsp::Complex<float>(  input[window_pos].real(),
                                                    input[window_pos].imag());
    }
}

void PhaseVocoder::DSPOffline(float* input, float* output, uint32_t buff_size, uint32_t channel)
{
    (void)output;
    uint32_t initial_val = m_buffer_size[channel];
    for (m_buffer_size[channel]; m_buffer_size[channel] < (buff_size + initial_val); m_buffer_size[channel]++)
    {
        m_complex_in_offline[channel][m_buffer_size[channel]] = dsp::Complex<float>(input[m_buffer_size[channel] - initial_val], 0.0f);
    }
}

void PhaseVocoder::DSPOnline(float* input, float* output, uint32_t buff_size, uint32_t channel)
{
    for (uint32_t i = 0; i < buff_size; i++)
    {
        m_complex_in_online[channel][i] = dsp::Complex<float>(input[i], 0.0f);
        m_complex_out_online[channel][i] = dsp::Complex<float>(0.0f, 0.0f);
    }
    for (uint32_t i = buff_size; i < 8*2048; i++)
    {
        m_complex_in_online[channel][i] = dsp::Complex<float>(0.0f, 0.0f);
        m_complex_out_online[channel][i] = dsp::Complex<float>(0.0f, 0.0f);
    }
    
    ProcessSegmentOnline(m_complex_in_online[channel], m_complex_out_online[channel], buff_size);

    for (uint32_t i = 0; i < buff_size; i++)
    {
        output[i] = m_complex_out_online[channel][i].real();
    }

    for (uint32_t i = 0; i < buff_size; i++)
    {
        m_test_buffer[channel][m_buffer_size_online[channel]++] += m_complex_out_online[channel][i];
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

void PhaseVocoder::ProcessBufferOffline()
{
    std::string output_file = LookupSafeWriteLocation() + LookupSafeFileName();

    // Zero output buffer before starting
    for (uint32_t i = 0; i < m_buffer_size[0]; i++)
    {
        m_complex_out_offline[0][i] = dsp::Complex<float>(0.0f, 0.0f);
    }
    for (uint32_t i = 0; i < m_buffer_size[1]; i++)
    {
        m_complex_out_offline[1][i] = dsp::Complex<float>(0.0f, 0.0f);
    }

    // Loop through segments
    ProcessSegmentOffline((m_complex_in_offline[0]), (m_complex_out_offline[0]), m_buffer_size[0]);
    ProcessSegmentOffline((m_complex_in_offline[1]), (m_complex_out_offline[1]), m_buffer_size[1]);

    // Write buffer to file
    CommitBufferOffline(output_file);
    CommitBufferOnline(output_file);
}

template <typename Word>
std::ostream& write_word(std::ostream& outs, Word value, unsigned size = sizeof(Word))
{
    for (; size; --size, value >>= 8)
        outs.put(static_cast <char> (value & 0xFF));
    return outs;
}

void PhaseVocoder::CommitBufferOffline(std::string output_file)
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
    write_word(f, (44100 * 16 * 2) / 8, 4);   // (Sample Rate * BitsPerSample * Channels) / 8
    write_word(f, 4, 2);                // data block size (size of two integer samples, one for each channel, in bytes)
    write_word(f, 16, 2);               // number of bits per sample (use a multiple of 8)

    // Write the data chunk header
    size_t data_chunk_pos = (size_t)f.tellp();
    f << "data----";  // (chunk size to be filled in later)

    for (uint32_t j = 0; j < m_buffer_size[0]; j++)
    {
        write_word(f, (int)(32760.0f * m_complex_out_offline[0][j].real()), 2);
        write_word(f, (int)(32760.0f * m_complex_out_offline[1][j].real()), 2);
    }

    // (We'll need the final file size to fix the chunk sizes above)
    size_t file_length = (size_t)f.tellp();

    // Fix the data chunk header to contain the data size
    f.seekp(data_chunk_pos + 4);
    write_word(f, file_length - data_chunk_pos + 8);

    // Fix the file header to contain the proper RIFF chunk size, which is (file size - 8) bytes
    f.seekp(0 + 4);
    write_word(f, file_length - 8, 4);
}

void PhaseVocoder::CommitBufferOnline(std::string output_file)
{
    static int i = 0;
    i++;
    std::ofstream f("C:\\school\\output_data_rt" + std::to_string(i) + ".wav", std::ios::binary);

    // Write the file headers
    f << "RIFF----WAVEfmt ";            // (chunk size to be filled in later)
    write_word(f, 16, 4);               // no extension data
    write_word(f, 1, 2);                // PCM - integer samples
    write_word(f, 2, 2);                // two channels (stereo file)
    write_word(f, 44100, 4);            // samples per second (Hz)
    write_word(f, (44100 * 16 * 2) / 8, 4);   // (Sample Rate * BitsPerSample * Channels) / 8
    write_word(f, 4, 2);                // data block size (size of two integer samples, one for each channel, in bytes)
    write_word(f, 16, 2);               // number of bits per sample (use a multiple of 8)

    // Write the data chunk header
    size_t data_chunk_pos = (size_t)f.tellp();
    f << "data----";  // (chunk size to be filled in later)

    for (uint32_t j = 0; j < m_buffer_size[0]; j++)
    {
        write_word(f, (int)(32760.0f * m_test_buffer[0][j].real()), 2);
        write_word(f, (int)(32760.0f * m_test_buffer[1][j].real()), 2);
    }

    // (We'll need the final file size to fix the chunk sizes above)
    size_t file_length = (size_t)f.tellp();

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

void PhaseVocoder::ProcessSegmentOffline(dsp::Complex<float>* input_buffer, dsp::Complex<float> * output_buffer, uint32_t segment_size)
{
    // Too small
    if (segment_size < m_window_size_offline)
    {
        for (uint32_t i = 0; i < segment_size; i++)
        {
            output_buffer[i] = input_buffer[i];
        }
        return;
    }

    uint32_t segment_edge = segment_size % m_window_size_offline;
    
    for (uint32_t i = m_hop_size_offline; i < m_window_size_offline; i += m_hop_size_offline)
    {
        ApplyProcessingOffline( input_buffer, m_complex_intermed_fw_offline, m_complex_intermed_rv_offline, output_buffer, WINDOW_SIZE_OFFLINE - i, i);
    }

    for (uint32_t i = 0; i < segment_size; i += m_hop_size_offline)
    {
        uint32_t window_size = (segment_size - i) > WINDOW_SIZE_OFFLINE ? WINDOW_SIZE_OFFLINE : segment_size - i;
        ApplyProcessingOffline( input_buffer + i, m_complex_intermed_fw_offline, m_complex_intermed_rv_offline, output_buffer + i, window_size, 0);
    }
}

void PhaseVocoder::ProcessSegmentOnline(dsp::Complex<float>* input_buffer, dsp::Complex<float> * output_buffer, uint32_t segment_size)
{
    // Too small
    if (segment_size < m_window_size_online)
    {
        for (uint32_t i = 0; i < segment_size; i++)
        {
            output_buffer[i] = input_buffer[i];
        }
        return;
    }

    for (uint32_t i = m_hop_size_online; i < m_window_size_online; i += m_hop_size_online)
    {
        ApplyProcessingOnline(input_buffer, m_complex_intermed_fw_online, m_complex_intermed_rv_online, output_buffer, WINDOW_SIZE_ONLINE - i, i);
    }

    for (uint32_t i = 0; i < segment_size; i += m_hop_size_online)
    {
        uint32_t window_size = (segment_size - i) > WINDOW_SIZE_ONLINE ? WINDOW_SIZE_ONLINE : segment_size - i;
        ApplyProcessingOnline(input_buffer + i, m_complex_intermed_fw_online, m_complex_intermed_rv_online, output_buffer + i, window_size, 0);
    }
}

void PhaseVocoder::ApplyProcessingOnline(dsp::Complex<float>* input,
    dsp::Complex<float>* intermed_fw,
    dsp::Complex<float>* intermed_rv,
    dsp::Complex<float>* output,
    uint32_t count,
    uint32_t window_start)
{
    float scaling_factor = (float)SCALING_FACTOR_ONLINE / 2;
    dsp::Complex<float> buff[FFT_SIZE_ONLINE];

    ApplyWindowFunctionOnline(input, buff, count, window_start);

    for (uint32_t i = count; i < FFT_SIZE_ONLINE; i++)
    {
        buff[i] = dsp::Complex<float>(0.0f, 0.0f);
    }

    m_forward_fft_online.perform(buff, intermed_fw, false);

    for (uint32_t i = 0; i < FFT_SIZE_ONLINE; i++)
    {
        intermed_fw[i] = dsp::Complex<float>(   intermed_fw[i].real() / (scaling_factor),
                                                intermed_fw[i].imag() / (scaling_factor));
    }

    Process();

    // Perform Phase locking (currently does nothing)
    PhaseLock();

    // Inverse FFT
    m_reverse_fft_online.perform(intermed_fw, intermed_rv, true);


    // Scale buffer back
    //ReScaleWindow(intermed, count, (float)FFT_SIZE);

    // Commit data to output buffer, since it is windowed we can just add
    WriteWindow(intermed_rv, output, count);
}