extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include "audio.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}
#include <cstring>
#include <iostream>
#include <memory>

#include "audio.h"

struct BufferContext {
  const uint8_t *data;
  size_t size;
  size_t pos;
};

static bool decode_from_format_ctx(AVFormatContext *fmt_ctx, int out_sample_rate, AudioData &out) {
  if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) return false;

  int audio_stream_index = -1;
  for (unsigned int i = 0; i < fmt_ctx->nb_streams; ++i) {
    if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      audio_stream_index = i;
      break;
    }
  }
  if (audio_stream_index < 0) return false;

  AVCodecParameters *codecpar = fmt_ctx->streams[audio_stream_index]->codecpar;
  AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
  AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(codec_ctx, codecpar);
  if (avcodec_open2(codec_ctx, codec, nullptr) < 0) return false;

  out_sample_rate = out_sample_rate <= 0 ? codec_ctx->sample_rate : out_sample_rate;
  SwrContext *swr = swr_alloc_set_opts(
    nullptr, AV_CH_LAYOUT_MONO, AV_SAMPLE_FMT_FLT, out_sample_rate,
    codec_ctx->channel_layout ? codec_ctx->channel_layout : av_get_default_channel_layout(codec_ctx->channels),
    codec_ctx->sample_fmt, codec_ctx->sample_rate, 0, nullptr);
  swr_init(swr);

  AVPacket *pkt = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();
  std::vector<float> all_samples;

  while (av_read_frame(fmt_ctx, pkt) >= 0) {
    if (pkt->stream_index == audio_stream_index) {
      if (avcodec_send_packet(codec_ctx, pkt) >= 0) {
        while (avcodec_receive_frame(codec_ctx, frame) == 0) {
          int out_samples = swr_get_out_samples(swr, frame->nb_samples);
          std::vector<float> buffer(out_samples);
          float *out_data[] = {buffer.data()};
          int converted = swr_convert(swr, (uint8_t **)out_data, out_samples, (const uint8_t **)frame->extended_data,
                                      frame->nb_samples);
          if (converted > 0) {
            all_samples.insert(all_samples.end(), buffer.begin(), buffer.begin() + converted);
          }
        }
      }
    }
    av_packet_unref(pkt);
  }

  out.sample_rate = out_sample_rate;
  out.samples = std::move(all_samples);

  av_frame_free(&frame);
  av_packet_free(&pkt);
  swr_free(&swr);
  avcodec_free_context(&codec_ctx);
  return true;
}

bool ReadAudio(const std::string &path_or_url, int out_sample_rate, AudioData &out) {
  avformat_network_init();

  AVFormatContext *fmt_ctx = nullptr;
  if (avformat_open_input(&fmt_ctx, path_or_url.c_str(), nullptr, nullptr) < 0) {
    std::cerr << "Failed to open: " << path_or_url << "\n";
    return false;
  }

  bool ok = decode_from_format_ctx(fmt_ctx, out_sample_rate, out);
  avformat_close_input(&fmt_ctx);
  return ok;
}

bool ReadAudio(const std::string &path_or_url, AudioData &out) { return ReadAudio(path_or_url, 0, out); }

bool ReadAudio(const std::vector<uint8_t> &buffer, int out_sample_rate, AudioData &out) {
  auto *opaque = new BufferContext{buffer.data(), buffer.size(), 0};

  auto read_fn = [](void *opaque, uint8_t *buf, int buf_size) -> int {
    auto *ctx = reinterpret_cast<BufferContext *>(opaque);
    int remain = ctx->size - ctx->pos;
    int to_copy = std::min(buf_size, remain);
    if (to_copy <= 0) return AVERROR_EOF;
    memcpy(buf, ctx->data + ctx->pos, to_copy);
    ctx->pos += to_copy;
    return to_copy;
  };

  auto seek_fn = [](void *opaque, int64_t offset, int whence) -> int64_t {
    auto *ctx = reinterpret_cast<BufferContext *>(opaque);
    if (whence == AVSEEK_SIZE) {
      return static_cast<int64_t>(ctx->size);
    }
    size_t new_pos;
    if (whence == SEEK_SET) {
      new_pos = offset;
    } else if (whence == SEEK_CUR) {
      new_pos = ctx->pos + offset;
    } else if (whence == SEEK_END) {
      new_pos = ctx->size + offset;
    } else {
      return -1;
    }
    if (new_pos > ctx->size) return -1;
    ctx->pos = new_pos;
    return static_cast<int64_t>(ctx->pos);
  };

  constexpr int avio_buf_size = 4096;
  uint8_t *avio_buffer = (uint8_t *)av_malloc(avio_buf_size);
  AVIOContext *avio_ctx = avio_alloc_context(avio_buffer, avio_buf_size, 0, opaque, read_fn, nullptr, seek_fn);

  AVFormatContext *fmt_ctx = avformat_alloc_context();
  fmt_ctx->pb = avio_ctx;
  fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

  bool ok = false;
  if (avformat_open_input(&fmt_ctx, nullptr, nullptr, nullptr) >= 0) {
    ok = decode_from_format_ctx(fmt_ctx, out_sample_rate, out);
    avformat_close_input(&fmt_ctx);
  } else {
    std::cerr << "Failed to open buffer input\n";
  }

  av_freep(&avio_ctx->buffer);
  avio_context_free(&avio_ctx);
  delete opaque;

  return ok;
}

bool ReadAudio(const std::vector<uint8_t> &buffer, AudioData &out) { return ReadAudio(buffer, 0, out); }
