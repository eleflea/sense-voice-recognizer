#include <iostream>

#include "recognizer.h"

using sherpa_onnx::cxx::OfflineRecognizer;
using sherpa_onnx::cxx::OfflineRecognizerResult;
using sherpa_onnx::cxx::OfflineStream;
using std::cerr;
using std::cout;

bool Recognizer::Init() {
  cout << "Loading model\n";
  recognizer_ = std::make_unique<OfflineRecognizer>(std::move(OfflineRecognizer::Create(config_)));
  if (!recognizer_->Get()) {
    cerr << "Failed to create recognizer. Please check your config.\n";
    return false;
  }
  cout << "Loading model done\n";
  return true;
}

OfflineRecognizerResult Recognizer::Recognize(const AudioData &wave) {
  OfflineStream stream = recognizer_->CreateStream();
  stream.AcceptWaveform(wave.sample_rate, wave.samples.data(), wave.samples.size());
  recognizer_->Decode(&stream);
  return recognizer_->GetResult(&stream);
}
