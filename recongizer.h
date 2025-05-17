#pragma once

#include <memory>

#include "audio.h"
#include "sherpa-onnx/c-api/cxx-api.h"

class Recongizer {
 private:
  std::unique_ptr<sherpa_onnx::cxx::OfflineRecognizer> recongizer_{nullptr};
  sherpa_onnx::cxx::OfflineRecognizerConfig config_;

 public:
  Recongizer(const sherpa_onnx::cxx::OfflineRecognizerConfig &config) : config_(config) {}
  ~Recongizer() = default;

  bool Init();
  sherpa_onnx::cxx::OfflineRecognizerResult Recognize(const AudioData &wave);
};
