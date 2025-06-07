cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(sense-voice-recognizer 
  VERSION 0.1
  DESCRIPTION "A C++ project for web-based ASR service using Sense Voice ONNX model for fast & efficient speech recognition."
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Threads REQUIRED)
find_library(DL_LIBRARY dl REQUIRED)

include(FetchContent)

FetchContent_Declare(
  sherpa-onnx
  GIT_REPOSITORY https://github.com/k2-fsa/sherpa-onnx.git
  GIT_TAG v1.11.4
)
set(SHERPA_ONNX_ENABLE_BINARY OFF CACHE BOOL "" FORCE)
set(SHERPA_ONNX_ENABLE_PORTAUDIO OFF CACHE BOOL "" FORCE)
set(SHERPA_ONNX_ENABLE_WEBSOCKET OFF CACHE BOOL "" FORCE)
set(SHERPA_ONNX_ENABLE_SPEAKER_DIARIZATION OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(sherpa-onnx)
FetchContent_GetProperties(onnxruntime)

FetchContent_Declare(
  Crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG v1.2.1.2
)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Crow)

FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(json)

set(sources
  app.cc
  audio.cc
  recognizer.cc
  task_manager.cc)

add_executable(sense-voice-recognizer ${sources})

target_link_libraries(sense-voice-recognizer PRIVATE Threads::Threads ${DL_LIBRARY})

target_link_libraries(sense-voice-recognizer PRIVATE
sherpa-onnx-cxx-api
"${onnxruntime_SOURCE_DIR}/lib")

target_link_libraries(sense-voice-recognizer PUBLIC Crow::Crow)

target_link_libraries(sense-voice-recognizer PRIVATE nlohmann_json::nlohmann_json)
