# Stage 1: Build
FROM debian:bullseye-slim AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    curl \
    libssl-dev \
    libpthread-stubs0-dev \
    libcurl4-openssl-dev \
    ca-certificates \
    libasio-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -B build -S . -D CMAKE_BUILD_TYPE=Release -D BUILD_SHARED_LIBS=ON && cmake --build build --parallel 4

# Stage 2: Runtime
FROM debian:bullseye-slim

RUN apt-get update && apt-get install -y \
    libstdc++6 \
    libgcc-s1 \
    libc6 \
    libcurl4 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/sense-voice-recognizer /app/build/sense-voice-recognizer
COPY --from=builder /app/.env /app/.env
COPY --from=builder /app/build/lib/libsherpa-onnx-cxx-api.so /usr/lib/
COPY --from=builder /app/build/lib/libsherpa-onnx-c-api.so /usr/lib/
COPY --from=builder /app/build/_deps/onnxruntime-src/lib/libonnxruntime.so /usr/lib/

ENV MODEL_WEIGHTS_DOCKER=models/model.int8.onnx
ENV MODEL_TOKENS_DOCKER=models/tokens.txt
ENV MODEL_USE_ITN=true
ENV MODEL_LANGUAGE=auto
ENV MODEL_NUM_THREADS=4

ENV AUDIO_RESAMPLE_RATE=16000
ENV MAX_PROCESSING_TIME=10
ENV MAX_QUEUE_CAPACITY=100

ENV WEB_HOST=0.0.0.0
ENV WEB_PORT=5000

EXPOSE ${WEB_PORT}

CMD ["./build/sense-voice-recognizer"]
