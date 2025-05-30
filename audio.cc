#define MINIAUDIO_IMPLEMENTATION  // Important: define this in exactly one .c or .cpp file
#include "include/miniaudio.h"

#include <iostream>
#include <fstream>  // For example usage

#include "audio.h"

AudioData ReadAudio(const std::vector<uint8_t> &file_buffer, std::optional<int32_t> target_sample_rate) {
  AudioData audio_data;

  if (file_buffer.empty()) {
    std::cerr << "Error: Input file buffer is empty." << std::endl;
    return audio_data;  // Return empty data
  }

  ma_decoder_config decoder_config = ma_decoder_config_init(ma_format_f32,  // We want output as float32
                                                            0,              // Channels (0 means auto-detect from file)
                                                            0  // Sample rate (0 means auto-detect from file)
  );

  // If a target sample rate is specified, set it in the config
  // miniaudio will handle the resampling during decoding.
  if (target_sample_rate.has_value() && *target_sample_rate > 0) {
    decoder_config.sampleRate = *target_sample_rate;
  }
  // else, it will use the native sample rate of the file.

  ma_decoder decoder;
  ma_result result = ma_decoder_init_memory(file_buffer.data(), file_buffer.size(), &decoder_config, &decoder);

  if (result != MA_SUCCESS) {
    std::cerr << "Failed to initialize audio decoder: " << ma_result_description(result) << std::endl;
    return audio_data;  // Return empty data
  }

  // Store the actual output sample rate and channels
  // If resampling was requested, decoder.outputSampleRate will be target_sample_rate.
  // Otherwise, it will be the native sample rate of the file.
  audio_data.sample_rate = decoder.outputSampleRate;
  audio_data.channels = decoder.outputChannels;

  if (audio_data.channels == 0 || audio_data.sample_rate == 0) {
    std::cerr << "Failed to determine audio channels or sample rate." << std::endl;
    ma_decoder_uninit(&decoder);
    return {};
  }

  // Estimate total frames to reserve space, can be rough
  ma_uint64 total_frames_estimate;
  result = ma_decoder_get_length_in_pcm_frames(&decoder, &total_frames_estimate);
  if (result == MA_SUCCESS && total_frames_estimate > 0) {
    audio_data.samples.reserve(total_frames_estimate * audio_data.channels);
  } else {
    // Fallback if length couldn't be determined, e.g., for some streams
    // Reserve a moderate amount, vector will grow if needed.
    audio_data.samples.reserve(44100 * 2 * 5);  // 5 seconds of stereo audio at 44.1kHz
  }

  // Buffer to read frames into
  // Reading in chunks is generally more efficient than one frame at a time
  const ma_uint64 FRAMES_PER_READ = 4096;  // Read 4096 frames at a time
  std::vector<float> temp_buffer(FRAMES_PER_READ * audio_data.channels);

  ma_uint64 frames_read_this_iteration;
  while (true) {
    result = ma_decoder_read_pcm_frames(&decoder, temp_buffer.data(), FRAMES_PER_READ, &frames_read_this_iteration);

    if (result != MA_SUCCESS && result != MA_AT_END) {  // MA_AT_END is not an error for reading
      std::cerr << "Failed to read PCM frames: " << ma_result_description(result) << std::endl;
      ma_decoder_uninit(&decoder);
      return {};  // Return empty data on read error
    }

    if (frames_read_this_iteration > 0) {
      audio_data.samples.insert(audio_data.samples.end(), temp_buffer.data(),
                                temp_buffer.data() + (frames_read_this_iteration * audio_data.channels));
    }

    // MA_AT_END means the end of the stream was reached.
    // frames_read_this_iteration == 0 also indicates no more data.
    if (result == MA_AT_END || frames_read_this_iteration == 0) {
      break;
    }
  }

  ma_decoder_uninit(&decoder);
  return audio_data;
}
