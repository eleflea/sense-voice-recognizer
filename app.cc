#include <crow.h>

#include <chrono>
#include <iostream>
#include <string>
#include <memory>

#include "config.h"
#include "audio.h"
#include "recognizer.h"
#include "task_manager.h"
#include "sherpa-onnx/c-api/cxx-api.h"

using sherpa_onnx::cxx::OfflineRecognizer;
using sherpa_onnx::cxx::OfflineRecognizerConfig;
using sherpa_onnx::cxx::OfflineRecognizerResult;
using sherpa_onnx::cxx::OfflineStream;
using std::cerr;
using std::cout;
using std::string;

OfflineRecognizerConfig GetRecognizerConfig(const Config &config) {
  OfflineRecognizerConfig recognizer_config;
  recognizer_config.model_config.sense_voice.model = config.get<string>("MODEL_WEIGHTS");
  recognizer_config.model_config.sense_voice.use_itn = config.get<bool>("MODEL_USE_ITN");
  recognizer_config.model_config.sense_voice.language = config.get<string>("MODEL_LANGUAGE");
  recognizer_config.model_config.tokens = config.get<string>("MODEL_TOKENS");
  recognizer_config.model_config.num_threads = config.get<int32_t>("MODEL_NUM_THREADS");

  return recognizer_config;
}

crow::SimpleApp SetupCrow(const std::shared_ptr<RecognitionTaskManager> task_manager, const Config &config) {
  crow::SimpleApp app;

  CROW_ROUTE(app, "/health")
  ([task_manager]() {
    crow::json::wvalue res;
    res["status"] = "ok";
    res["queue_size"] = task_manager->getQueueSize();
    return res;
  });

  CROW_ROUTE(app, "/asr").methods("POST"_method)([task_manager, &config](const crow::request &req) {
    const auto begin = std::chrono::steady_clock::now();

    if (task_manager->getQueueSize() >= config.get<int32_t>("MAX_QUEUE_CAPACITY")) {
      return crow::response(503, "Server is busy, please try again later.");
    }

    crow::multipart::mp_map part_map;
    try {
      crow::multipart::message multipart_req(req);
      part_map = multipart_req.part_map;
    } catch (const std::exception &e) {
      return crow::response(400, std::string("Multipart parse error: ") + e.what());
    }

    std::string language = "auto";
    std::vector<uint8_t> file_data;

    for (auto &[key, part] : part_map) {
      if (key == "language" && !part.body.empty()) {
        language = part.body;
      } else if (key == "file") {
        file_data = std::vector<uint8_t>(part.body.begin(), part.body.end());
      }
    }

    if (file_data.empty()) {
      return crow::response(400, "Missing 'file' field.");
    }

    auto wave = ReadAudio(file_data, config.get<int32_t>("AUDIO_RESAMPLE_RATE"));
    if (!wave.isValid()) {
      return crow::response(400, "Failed to read audio file.");
    }

    auto future = task_manager->submitTask(wave);

    if (future.wait_for(std::chrono::seconds(config.get<int32_t>("MAX_PROCESSING_TIME"))) !=
        std::future_status::ready) {
      return crow::response(504, "Timeout while processing");
    }

    auto asr_result = future.get();
    const auto end = std::chrono::steady_clock::now();
    const float elapsed_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() / 1000.;
    float duration = wave.samples.size() / static_cast<float>(wave.sample_rate);
    float rtf = duration / elapsed_seconds;
    cout << "RTF = " << duration << "s / " << elapsed_seconds << "s = " << rtf << "\n";

    return crow::response(200, asr_result.json);
  });

  return app;
}

int32_t main() {
  Config config;

  auto recognizer_config = GetRecognizerConfig(config);
  auto recognizer = std::make_shared<Recognizer>(recognizer_config);
  if (!recognizer) {
    cerr << "Failed to create recognizer with config.\n";
    return -1;
  }
  recognizer->Init();

  auto task_manager = std::make_shared<RecognitionTaskManager>(
    [recognizer](const AudioData &wave) { return recognizer->Recognize(wave); });

  auto app = SetupCrow(task_manager, config);
  app.bindaddr(config.get<string>("WEB_HOST")).port(config.get<int32_t>("WEB_PORT")).multithreaded().run();

  return 0;
}
