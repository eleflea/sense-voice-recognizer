#include <iostream>

#include "recongizer.h"

using sherpa_onnx::cxx::OfflineRecognizer;
using sherpa_onnx::cxx::OfflineRecognizerResult;
using sherpa_onnx::cxx::OfflineStream;
using std::cerr;
using std::cout;

bool Recongizer::Init() {
  cout << "Loading model\n";
  recongizer_ = std::make_unique<OfflineRecognizer>(std::move(OfflineRecognizer::Create(config_)));
  if (!recongizer_->Get()) {
    cerr << "Failed to create recongizer. Please check your config.\n";
    return false;
  }
  cout << "Loading model done\n";
  return true;
}

OfflineRecognizerResult Recongizer::Recognize(const AudioData &wave) {
  OfflineStream stream = recongizer_->CreateStream();
  stream.AcceptWaveform(wave.sample_rate, wave.samples.data(), wave.samples.size());
  recongizer_->Decode(&stream);
  return recongizer_->GetResult(&stream);
}
