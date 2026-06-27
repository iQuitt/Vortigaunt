#pragma once

#include <string>
#include <vector>

#include <cstdint>

/**
 * Audio conversion settings for output WAV file
 */
struct AudioConvertSettings
{
    uint32_t sampleRate = 22050;   // Target sample rate in Hz
    uint16_t bitDepth = 16;         // Target bit depth (8 or 16)
    uint16_t channels = 1;          // Target channels (1 = mono, 2 = stereo)
};

/**
 * Result codes for audio conversion operations
 */
enum AudioConvertResult
{
    AUDIO_CONVERT_OK = 0,
    AUDIO_CONVERT_FILE_NOT_FOUND = 1,
    AUDIO_CONVERT_INVALID_FORMAT = 2,
    AUDIO_CONVERT_DECODE_FAILED = 3,
    AUDIO_CONVERT_ENCODE_FAILED = 4,
    AUDIO_CONVERT_IO_ERROR = 5,
    AUDIO_CONVERT_UNSUPPORTED_FORMAT = 6,
};

/**
 * Audio format detection
 */
enum AudioFormat
{
    AUDIO_FORMAT_UNKNOWN = 0,
    AUDIO_FORMAT_MP3,
    AUDIO_FORMAT_OGG,
    AUDIO_FORMAT_WAV,
};

/**
 * AudioConverter class for converting MP3/OGG files to WAV format
 * 
 * Supports:
 * - MP3 to WAV conversion (via dr_mp3)
 * - OGG Vorbis to WAV conversion (via stb_vorbis)
 * - Resampling to target sample rate
 * - Bit depth conversion (8-bit or 16-bit)
 * - Stereo to mono downmixing (always outputs mono)
 */
class AudioConverter
{
public:
    AudioConverter();
    ~AudioConverter();



    /**
     * Convert any supported audio file to WAV format
     * Auto-detects input format based on file extension
     * 
     * @param inputPath Path to input audio file (MP3 or OGG)
     * @param outputPath Path to output WAV file
     * @param settings Conversion settings (sample rate, bit depth, channels)
     * @return AudioConvertResult indicating success or error type
     */
    AudioConvertResult convertToWav(const std::string& inputPath,
                                     const std::string& outputPath,
                                     const AudioConvertSettings& settings = AudioConvertSettings());

    /**
     * Convert MP3 file to WAV format
     */
    AudioConvertResult convertMp3ToWav(const std::string& inputPath,
                                        const std::string& outputPath,
                                        const AudioConvertSettings& settings = AudioConvertSettings());

    /**
     * Convert OGG Vorbis file to WAV format
     */
    AudioConvertResult convertOggToWav(const std::string& inputPath,
                                        const std::string& outputPath,
                                        const AudioConvertSettings& settings = AudioConvertSettings());

    /**
     * Convert WAV file to target WAV format
     */
    AudioConvertResult convertWavToWav(const std::string& inputPath,
                                        const std::string& outputPath,
                                        const AudioConvertSettings& settings = AudioConvertSettings());

    /**
     * Detect audio format from file extension
     */
    static AudioFormat detectFormat(const std::string& filePath);

    /**
     * Get string description for result code
     */
    static std::string getResultString(AudioConvertResult result);

private:

    /**
     * Resample audio data from source sample rate to target sample rate
     * Uses linear interpolation for simplicity
     */
    std::vector<float> resample(const float* inputSamples, size_t inputFrameCount, uint32_t inputSampleRate,  uint32_t outputSampleRate, uint32_t channels);

    /**
     * Mix stereo audio to mono by averaging left and right channels
     */
    std::vector<float> stereoToMono(const float* inputSamples, size_t frameCount);

    /**
     * Convert float samples (-1.0 to 1.0) to 8-bit unsigned PCM (0-255)
     */
    std::vector<uint8_t> floatTo8Bit(const float* samples, size_t sampleCount);

    /**
     * Convert float samples (-1.0 to 1.0) to 16-bit signed PCM
     */
    std::vector<int16_t> floatTo16Bit(const float* samples, size_t sampleCount);

    /**
     * Write WAV file with given PCM data
     * Supports both 8-bit and 16-bit output
     */
    bool writeWavFile(const std::string& outputPath, const void* pcmData,  size_t sampleCount, uint32_t sampleRate, uint16_t bitDepth, uint16_t channels);

    /**
     * Common post-processing pipeline: resample, stereo->mono, bit-depth convert, write WAV.
     * Called by both convertMp3ToWav and convertOggToWav after decoding.
     */
    AudioConvertResult processAndWriteWav(const float* samples,
                                          size_t frameCount,
                                          uint32_t sampleRate,
                                          uint32_t channels,
                                          const AudioConvertSettings& settings,
                                          const std::string& outputPath);
};
