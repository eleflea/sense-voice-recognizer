#pragma once

#include <memory>

#include "audio.h"
#include "sherpa-onnx/c-api/cxx-api.h"

class Recognizer {
 private:
  std::unique_ptr<sherpa_onnx::cxx::OfflineRecognizer> recognizer_{nullptr};
  sherpa_onnx::cxx::OfflineRecognizerConfig config_;

 public:
  Recognizer(const sherpa_onnx::cxx::OfflineRecognizerConfig &config) : config_(config) {}
  ~Recognizer() = default;

  bool Init();
  sherpa_onnx::cxx::OfflineRecognizerResult Recognize(const AudioData &wave);
};
