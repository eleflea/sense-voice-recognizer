cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(sense-voice-recongizer 
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
set(sherpa-onnx_SHERPA_ONNX_ENABLE_BINARY OFF)
set(sherpa-onnx_BUILD_SHARED_LIBS ON)
FetchContent_MakeAvailable(sherpa-onnx)
FetchContent_GetProperties(onnxruntime)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)

FetchContent_Declare(
  Crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG v1.2.1.2
)
set(crow_CROW_BUILD_EXAMPLES OFF)
set(crow_CROW_BUILD_TESTS OFF)
FetchContent_MakeAvailable(Crow)

set(sources
  app.cc
  audio.cc
  recongizer.cc
  task_manager.cc)

add_executable(sense-voice-recongizer ${sources})

target_link_libraries(sense-voice-recongizer PRIVATE Threads::Threads ${DL_LIBRARY})

target_link_libraries(sense-voice-recongizer PRIVATE
  sherpa-onnx-cxx-api
  "${onnxruntime_SOURCE_DIR}/lib")

target_link_libraries(sense-voice-recongizer PUBLIC yaml-cpp::yaml-cpp)

target_link_libraries(sense-voice-recongizer PUBLIC Crow::Crow)
