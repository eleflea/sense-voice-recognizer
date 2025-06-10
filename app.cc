#include <crow.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <memory>
#include <set>
#include <optional>

#include "config.h"
#include "audio.h"
#include "recognizer.h"
#include "task_manager.h"
#include "middlewares.h"
#include "sherpa-onnx/c-api/cxx-api.h"

using json = nlohmann::json;
using sherpa_onnx::cxx::OfflineRecognizer;
using sherpa_onnx::cxx::OfflineRecognizerConfig;
using sherpa_onnx::cxx::OfflineRecognizerResult;
using sherpa_onnx::cxx::OfflineStream;
using std::cerr;
using std::cout;
using std::string;

struct RoundedFloatVector {
  std::vector<float> data;
};

void to_json(json &j, const RoundedFloatVector &rfv) {
  j = json::array();
  for (auto num : rfv.data) {
    double rounded = static_cast<int>(num * 100 + (num >= 0 ? 0.5 : -0.5)) / 100.0;  // Round to 2 decimal places
    j.push_back(rounded);
  }
}

static std::set<string> NO_AUDIO_PUNCTUATION = {"!", "'", ",", ".", ";", "?", "~"};

OfflineRecognizerConfig GetRecognizerConfig(const Config &config) {
  OfflineRecognizerConfig recognizer_config;
  recognizer_config.model_config.sense_voice.model = config.get<string>("MODEL_WEIGHTS_LOCAL");
  recognizer_config.model_config.sense_voice.use_itn = config.get<bool>("MODEL_USE_ITN");
  recognizer_config.model_config.sense_voice.language = config.get<string>("MODEL_LANGUAGE");
  recognizer_config.model_config.tokens = config.get<string>("MODEL_TOKENS_LOCAL");
  recognizer_config.model_config.num_threads = config.get<int32_t>("MODEL_NUM_THREADS");

  return recognizer_config;
}

crow::App<BearerAuthMiddleware> SetupCrow(const std::shared_ptr<RecognitionTaskManager> task_manager,
                                          const Config &config) {
  std::optional<std::string> bearer_token = std::nullopt;
  if (config.has("BEARER_TOKEN")) {
    bearer_token.emplace(config.get<std::string>("BEARER_TOKEN"));
  }
  BearerAuthMiddleware bearer_auth_middleware(bearer_token);

  crow::App<BearerAuthMiddleware> app(bearer_auth_middleware);

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
    auto is_no_audio = std::all_of(asr_result.tokens.begin(), asr_result.tokens.end(),
                                   [](const std::string &token) { return NO_AUDIO_PUNCTUATION.count(token) > 0; });
    if (is_no_audio) {
      asr_result.text = "";
      asr_result.tokens.clear();
      asr_result.timestamps.clear();
    }
    json result_json = {
      {"status", is_no_audio ? "no_audio" : "normal"},
      {"lang", asr_result.lang},
      {"emotion", asr_result.emotion},
      {"event", asr_result.event},
      {"text", asr_result.text},
      {"timestamps", RoundedFloatVector{asr_result.timestamps}},
      {"tokens", asr_result.tokens},
    };

    const auto end = std::chrono::steady_clock::now();
    const float elapsed_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() / 1000.;
    float duration = wave.samples.size() / static_cast<float>(wave.sample_rate);
    float rtf = duration / elapsed_seconds;
    cout << "RTF = " << duration << "s / " << elapsed_seconds << "s = " << rtf << "\n";

    return crow::response(200, result_json.dump());
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
