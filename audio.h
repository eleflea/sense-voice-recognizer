#pragma once

#include <string>
#include <vector>
#include <optional>

struct AudioData {
  std::vector<float> samples;
  int32_t sample_rate = 0;
  int32_t channels = 0;

  // Helper to check if data is valid
  bool isValid() const { return !samples.empty() && sample_rate > 0 && channels > 0; }
};

AudioData ReadAudio(const std::vector<uint8_t> &file_buffer, std::optional<int32_t> target_sample_rate = std::nullopt);
