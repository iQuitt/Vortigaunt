/**
 * AudioConvert.cpp - MP3/OGG to WAV audio converter
 * 
 * Uses dr_mp3 for MP3 decoding, stb_vorbis for OGG decoding,
 * and dr_wav for WAV writing.
 * 
 */

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include "core/VortigauntLog.h"
#include "stb_vorbis.c"

#include "AudioConvert.h"

#include <cstdio>

#include <algorithm>
#include <cmath>
#include <filesystem>



AudioConverter::AudioConverter()
{
}

AudioConverter::~AudioConverter()
{
}

AudioFormat AudioConverter::detectFormat(const std::string& filePath)
{
    std::filesystem::path path(filePath);
    std::string ext = path.extension().string();
    
    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".mp3")
        return AUDIO_FORMAT_MP3;
    else if (ext == ".ogg")
        return AUDIO_FORMAT_OGG;
    else if (ext == ".wav")
        return AUDIO_FORMAT_WAV;
    
    return AUDIO_FORMAT_UNKNOWN;
}

std::string AudioConverter::getResultString(AudioConvertResult result)
{
    switch (result)
    {
    case AUDIO_CONVERT_OK:
        return "Success";
    case AUDIO_CONVERT_FILE_NOT_FOUND:
        return "File not found";
    case AUDIO_CONVERT_INVALID_FORMAT:
        return "Invalid audio format";
    case AUDIO_CONVERT_DECODE_FAILED:
        return "Failed to decode audio";
    case AUDIO_CONVERT_ENCODE_FAILED:
        return "Failed to encode WAV";
    case AUDIO_CONVERT_IO_ERROR:
        return "I/O error";
    case AUDIO_CONVERT_UNSUPPORTED_FORMAT:
        return "Unsupported audio format";
    default:
        return "Unknown error";
    }
}

AudioConvertResult AudioConverter::convertToWav(const std::string& inputPath,const std::string& outputPath, const AudioConvertSettings& settings)
{
    AudioFormat format = detectFormat(inputPath);
    
    switch (format)
    {
    case AUDIO_FORMAT_MP3:
        return convertMp3ToWav(inputPath, outputPath, settings);
    case AUDIO_FORMAT_OGG:
        return convertOggToWav(inputPath, outputPath, settings);
    case AUDIO_FORMAT_WAV:
        return convertWavToWav(inputPath, outputPath, settings);
    default:
        VortigauntLog::LogF("%s: Unsupported audio format: %s", __func__, inputPath.c_str());
        return AUDIO_CONVERT_UNSUPPORTED_FORMAT;
    }
}

AudioConvertResult AudioConverter::convertMp3ToWav(const std::string& inputPath,
                                                    const std::string& outputPath,
                                                    const AudioConvertSettings& settings)
{
    VortigauntLog::LogF("AudioConvert: Converting MP3: %s -> %s", inputPath.c_str(), outputPath.c_str());
    VortigauntLog::LogF("AudioConvert: Target: %d Hz, %d-bit, Mono", 
             settings.sampleRate, settings.bitDepth);

    if (!std::filesystem::exists(inputPath))
    {
        VortigauntLog::LogF("%s: File not found: %s", __func__, inputPath.c_str());
        return AUDIO_CONVERT_FILE_NOT_FOUND;
    }

    // Decode MP3 file
    drmp3_config mp3Config;
    drmp3_uint64 totalFrameCount;
    float* mp3Samples = drmp3_open_file_and_read_pcm_frames_f32(
        inputPath.c_str(), &mp3Config, &totalFrameCount, nullptr);
    
    if (!mp3Samples)
    {
        VortigauntLog::LogF("%s: Failed to decode MP3 file", __func__);
        return AUDIO_CONVERT_DECODE_FAILED;
    }

    VortigauntLog::LogF("AudioConvert: MP3 decoded: %llu frames, %d Hz, %d channels",
             totalFrameCount, mp3Config.sampleRate, mp3Config.channels);

    AudioConvertResult result = processAndWriteWav(
        mp3Samples,
        static_cast<size_t>(totalFrameCount),
        mp3Config.sampleRate,
        mp3Config.channels,
        settings,
        outputPath);

    drmp3_free(mp3Samples, nullptr);
    return result;
}

AudioConvertResult AudioConverter::convertOggToWav(const std::string& inputPath,
                                                    const std::string& outputPath,
                                                    const AudioConvertSettings& settings)
{
    VortigauntLog::LogF("AudioConvert: Converting OGG: %s -> %s", inputPath.c_str(), outputPath.c_str());
    VortigauntLog::LogF("AudioConvert: Target: %d Hz, %d-bit, Mono", 
             settings.sampleRate, settings.bitDepth);

    if (!std::filesystem::exists(inputPath))
    {
        VortigauntLog::LogF("%s: File not found: %s", __func__, inputPath.c_str());
        return AUDIO_CONVERT_FILE_NOT_FOUND;
    }

    // Decode OGG Vorbis file
    int oggChannels, oggSampleRate;
    short* oggSamples;
    int sampleCount = stb_vorbis_decode_filename(
        inputPath.c_str(), &oggChannels, &oggSampleRate, &oggSamples);
    
    if (sampleCount <= 0)
    {
        VortigauntLog::LogF("%s: Failed to decode OGG file", __func__);
        return AUDIO_CONVERT_DECODE_FAILED;
    }

    VortigauntLog::LogF("AudioConvert: OGG decoded: %d samples, %d Hz, %d channels",
             sampleCount, oggSampleRate, oggChannels);

    // Convert short samples to float for processing
    size_t totalOggSamples = static_cast<size_t>(sampleCount) * oggChannels;
    std::vector<float> floatSamples(totalOggSamples);
    for (size_t i = 0; i < totalOggSamples; i++)
    {
        floatSamples[i] = oggSamples[i] / 32768.0f;
    }
    free(oggSamples);

    AudioConvertResult result = processAndWriteWav(
        floatSamples.data(),
        static_cast<size_t>(sampleCount),
        static_cast<uint32_t>(oggSampleRate),
        static_cast<uint32_t>(oggChannels),
        settings,
        outputPath);

    return result;
}

AudioConvertResult AudioConverter::convertWavToWav(const std::string& inputPath,
                                                    const std::string& outputPath,
                                                    const AudioConvertSettings& settings)
{
    VortigauntLog::LogF("AudioConvert: Converting WAV: %s -> %s", inputPath.c_str(), outputPath.c_str());
    VortigauntLog::LogF("AudioConvert: Target: %d Hz, %d-bit, Mono", 
             settings.sampleRate, settings.bitDepth);

    if (!std::filesystem::exists(inputPath))
    {
        VortigauntLog::LogF("%s: File not found: %s", __func__, inputPath.c_str());
        return AUDIO_CONVERT_FILE_NOT_FOUND;
    }

    // Decode WAV file
    unsigned int wavChannels;
    unsigned int wavSampleRate;
    drwav_uint64 totalFrameCount;
    float* wavSamples = drwav_open_file_and_read_pcm_frames_f32(
        inputPath.c_str(), &wavChannels, &wavSampleRate, &totalFrameCount, nullptr);
    
    if (!wavSamples)
    {
        VortigauntLog::LogF("%s: Failed to decode WAV file", __func__);
        return AUDIO_CONVERT_DECODE_FAILED;
    }

    VortigauntLog::LogF("AudioConvert: WAV decoded: %llu frames, %d Hz, %d channels",
             totalFrameCount, wavSampleRate, wavChannels);

    AudioConvertResult result = processAndWriteWav(
        wavSamples,
        static_cast<size_t>(totalFrameCount),
        wavSampleRate,
        wavChannels,
        settings,
        outputPath);

    drwav_free(wavSamples, nullptr);
    return result;
}

AudioConvertResult AudioConverter::processAndWriteWav(const float* samples, size_t frameCount, uint32_t sampleRate, uint32_t channels, const AudioConvertSettings& settings, const std::string& outputPath)
{
    const float* currentSamplePtr = samples;
    uint32_t currentSampleRate = sampleRate;
    uint32_t currentChannels = channels;
    size_t currentFrameCount = frameCount;

    // Resample if sample rate differs
    std::vector<float> resampledSamples;
    if (currentSampleRate != settings.sampleRate)
    {
        VortigauntLog::LogF("AudioConvert: Resampling from %d Hz to %d Hz", currentSampleRate, settings.sampleRate);
        resampledSamples = resample(currentSamplePtr, currentFrameCount,
                                     currentSampleRate, settings.sampleRate, currentChannels);
        currentSamplePtr = resampledSamples.data();
        currentFrameCount = resampledSamples.size() / currentChannels;
        currentSampleRate = settings.sampleRate;
    }

    // Convert stereo to mono if needed
    std::vector<float> monoSamples;
    if (currentChannels == 2 && settings.channels == 1)
    {
        VortigauntLog::LogF("AudioConvert: Converting stereo to mono");
        monoSamples = stereoToMono(currentSamplePtr, currentFrameCount);
        currentSamplePtr = monoSamples.data();
        currentChannels = 1;
    }

    size_t totalSampleCount = currentFrameCount * currentChannels;

    // Convert to target bit depth and write
    bool writeSuccess = false;
    if (settings.bitDepth == 8)
    {
        VortigauntLog::LogF("AudioConvert: Converting to 8-bit PCM");
        std::vector<uint8_t> pcm8 = floatTo8Bit(currentSamplePtr, totalSampleCount);
        writeSuccess = writeWavFile(outputPath, pcm8.data(), totalSampleCount,
                                     settings.sampleRate, 8, settings.channels);
    }
    else
    {
        VortigauntLog::LogF("AudioConvert: Converting to 16-bit PCM");
        std::vector<int16_t> pcm16 = floatTo16Bit(currentSamplePtr, totalSampleCount);
        writeSuccess = writeWavFile(outputPath, pcm16.data(), totalSampleCount,
                                     settings.sampleRate, 16, settings.channels);
    }

    if (!writeSuccess)
    {
        VortigauntLog::LogF("%s: Failed to write WAV file", __func__);
        return AUDIO_CONVERT_ENCODE_FAILED;
    }

    VortigauntLog::LogF("AudioConvert: Conversion complete: %s", outputPath.c_str());
    return AUDIO_CONVERT_OK;
}

std::vector<float> AudioConverter::resample(const float* inputSamples, size_t inputFrameCount, uint32_t inputSampleRate, uint32_t outputSampleRate, uint32_t channels)
{
    // Calculate output frame count
    double ratio = static_cast<double>(outputSampleRate) / static_cast<double>(inputSampleRate);
    size_t outputFrameCount = static_cast<size_t>(inputFrameCount * ratio);
    
    std::vector<float> output(outputFrameCount * channels);
    
    // Linear interpolation resampling
    for (size_t outFrame = 0; outFrame < outputFrameCount; outFrame++)
    {
        double inputPos = outFrame / ratio;
        size_t inputFrame0 = static_cast<size_t>(inputPos);
        size_t inputFrame1 = std::min(inputFrame0 + 1, inputFrameCount - 1);
        double t = inputPos - inputFrame0;
        
        for (uint32_t ch = 0; ch < channels; ch++)
        {
            float sample0 = inputSamples[inputFrame0 * channels + ch];
            float sample1 = inputSamples[inputFrame1 * channels + ch];
            output[outFrame * channels + ch] = static_cast<float>(sample0 + (sample1 - sample0) * t);
        }
    }
    
    return output;
}

std::vector<float> AudioConverter::stereoToMono(const float* inputSamples, size_t frameCount)
{
    std::vector<float> output(frameCount);
    
    for (size_t i = 0; i < frameCount; i++)
    {
        output[i] = (inputSamples[i * 2] + inputSamples[i * 2 + 1]) * 0.5f;
    }
    
    return output;
}

std::vector<uint8_t> AudioConverter::floatTo8Bit(const float* samples, size_t sampleCount)
{
    std::vector<uint8_t> output(sampleCount);
    
    for (size_t i = 0; i < sampleCount; i++)
    {
        float clamped = std::max(-1.0f, std::min(1.0f, samples[i]));
        output[i] = static_cast<uint8_t>((clamped + 1.0f) * 127.5f);
    }
    
    return output;
}

std::vector<int16_t> AudioConverter::floatTo16Bit(const float* samples, size_t sampleCount)
{
    std::vector<int16_t> output(sampleCount);
    
    for (size_t i = 0; i < sampleCount; i++)
    {
        float clamped = std::max(-1.0f, std::min(1.0f, samples[i]));
        output[i] = static_cast<int16_t>(clamped * 32767.0f); // 16 bit
    }
    
    return output;
}

bool AudioConverter::writeWavFile(const std::string& outputPath, const void* pcmData, size_t sampleCount, uint32_t sampleRate, uint16_t bitDepth, uint16_t channels)
{
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = channels;
    format.sampleRate = sampleRate;
    format.bitsPerSample = bitDepth;
    
    drwav wav;
    if (!drwav_init_file_write(&wav, outputPath.c_str(), &format, nullptr))
    {
        VortigauntLog::LogF("%s: Failed to open WAV file for writing: %s", __func__, outputPath.c_str());
        return false;
    }
    
    size_t frameCount = sampleCount / channels;
    drwav_uint64 framesWritten = drwav_write_pcm_frames(&wav, frameCount, pcmData);
    
    drwav_uninit(&wav);
    
    if (framesWritten != frameCount)
    {
        VortigauntLog::LogF("%s: Only wrote %llu of %zu frames", __func__, framesWritten, frameCount);
        return false;
    }
    
    return true;
}
