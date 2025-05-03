#ifndef AUDIO_H
#define AUDIO_H

#include <string>
#include <vector>

struct AudioData {
  std::vector<float> samples;
  int32_t sample_rate = 0;
};

bool ReadAudio(const std::string &path_or_url, AudioData &out);
bool ReadAudio(const std::string &path_or_url, int out_sample_rate, AudioData &out);
bool ReadAudio(const std::vector<uint8_t> &buffer, AudioData &out);
bool ReadAudio(const std::vector<uint8_t> &buffer, int out_sample_rate, AudioData &out);

#endif  // AUDIO_H
