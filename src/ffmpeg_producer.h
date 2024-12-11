#pragma once

#include "essence_block.h"
#include "queue_thread_safe.h"
#include <iostream>
extern "C" {
#include <libavformat/avformat.h>
}
#include <nlohmann/json.hpp>

#define MAX_ESSENCE_PAYLOAD_SIZE 1024 * 1024 * 4

// Function to announce the block's metadata information (header)
void announce_block_info(EssenceBlock *_block) {
  std::string type;
  if(_block->essence_type == AVMEDIA_TYPE_VIDEO) {
    type = "video";
  }
  else if(_block->essence_type == AVMEDIA_TYPE_AUDIO) {
    type = "audio";
  }
  else if(_block->essence_type == AVMEDIA_TYPE_SUBTITLE) {
    type = "subtitle";
  }
  else {
    type = "unknown";
  }

  std::cout << "Announcing block info: " << "Stream " << (int) _block->stream_index << ", Type: " << type << ", Payload Size: " << _block->payload_size << ", Timestamp: " << _block->timestamp << std::endl;
}

void setEssenceBlock(EssenceBlock *_block, int _programIndex, AVPacket &_packet, AVStream *_stream) {
  // Set block metadata
  _block->essence_type = EssenceType::ESSENCE_TYPE_ED;
  _block->program_index = _programIndex;
  _block->stream_index = _packet.stream_index;
  _block->stream_type = _stream->codecpar->codec_type;
  _block->timestamp = _packet.pts;
  _block->payload_size = _packet.size;

  // payload
  uint8_t* payload = (uint8_t *)(_block + 1);
  std::memcpy(payload, _packet.data, _packet.size);
}

void setEssenceAnnouncementBlock(EssenceBlock* _block, int _programIndex, nlohmann::json &_jsonInfo) {
  std::string serialized_json = _jsonInfo.dump();
  std::vector<uint8_t> data(serialized_json.begin(), serialized_json.end());

  // Set block metadata
  _block->essence_type = EssenceType::ESSENCE_TYPE_EA;
  _block->program_index = _programIndex;
  _block->stream_index = 0xff;
  _block->stream_type = 0xff;
  _block->timestamp = 0;
  _block->payload_size = (uint32_t) data.size();

  // payload
  uint8_t* payload = (uint8_t*)(_block + 1);
  std::memcpy(payload, &(data[0]), data.size());
}

struct FFMPEGProducerParams {
  int programIndex = 0;
};

void ffmpeg_producer(FFMPEGProducerParams &_params, ThreadSafeQueue<EssenceBlock *> &_queue, const char *_inputFile) {
  // Initialize FFmpeg libraries
  avformat_network_init();

  AVFormatContext* formatContext = nullptr;

  // Open the input media file
  if(avformat_open_input(&formatContext, _inputFile, nullptr, nullptr) < 0) {
    std::cerr << "Error: Couldn't open input file: " << _inputFile << std::endl;
    return;
  }

  // Retrieve stream information
  if(avformat_find_stream_info(formatContext, nullptr) < 0) {
    std::cerr << "Error: Couldn't find stream information!" << std::endl;
    avformat_close_input(&formatContext);
    return;
  }

  // Print detailed information about the input file
  av_dump_format(formatContext, 0, _inputFile, false);

  // build json
  nlohmann::json stream_info;

  // Iterate through the streams and handle each one
  for(unsigned int i = 0; i < formatContext->nb_streams; i++) {
    AVStream* stream = formatContext->streams[i];
    AVCodecParameters* codecParams = stream->codecpar;

    // Identify stream types (audio, video, subtitle, etc.)
    switch(codecParams->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
      std::cout << "Video stream " << i << std::endl;
      std::cout << "  Codec: " << avcodec_get_name(codecParams->codec_id) << std::endl;
      break;
    case AVMEDIA_TYPE_AUDIO:
      std::cout << "Audio stream " << i << std::endl;
      std::cout << "  Codec: " << avcodec_get_name(codecParams->codec_id) << std::endl;
      break;
    case AVMEDIA_TYPE_SUBTITLE:
      std::cout << "Subtitle stream " << i << std::endl;
      break;
    default:
      std::cout << "Unknown stream " << i << std::endl;
      break;
    }

    // json
    nlohmann::json stream_json;
    stream_json["index"] = stream->index;
    stream_json["type"] = av_get_media_type_string(codecParams->codec_type) ? av_get_media_type_string(codecParams->codec_type) : "unknown";
    stream_json["codec"] = avcodec_get_name(codecParams->codec_id);
    if(codecParams->codec_type == AVMEDIA_TYPE_VIDEO) {
      stream_json["width"] = codecParams->width;
      stream_json["height"] = codecParams->height;
      stream_json["frame_rate"] = av_q2d(stream->r_frame_rate);
    }
    else if(codecParams->codec_type == AVMEDIA_TYPE_AUDIO) {
      stream_json["sample_rate"] = codecParams->sample_rate;
      stream_json["channels"] = codecParams->ch_layout.nb_channels;
    }
    stream_json["bit_rate"] = codecParams->bit_rate;
    stream_info["streams"].push_back(stream_json);
  }

  stream_info["program_index"] = _params.programIndex;

  // Output the JSON
  std::cout << stream_info.dump(4) << std::endl;

  // Build EA
  EssenceBlock* block = createEssenceBlock(MAX_ESSENCE_PAYLOAD_SIZE);
  setEssenceAnnouncementBlock(block, _params.programIndex, stream_info);
  _queue.push(block);

  // Read the packets
  AVPacket packet;
  while(av_read_frame(formatContext, &packet) >= 0) {
    // Create an EssenceBlock for the current packet
    EssenceBlock *block = createEssenceBlock(MAX_ESSENCE_PAYLOAD_SIZE);    
    setEssenceBlock(block, _params.programIndex, packet, formatContext->streams[packet.stream_index]);

    // Announce the block info
    announce_block_info(block);

    // Register
    _queue.push(block);

    // Free the packet
    av_packet_unref(&packet);
  }

  // Clean up and close the input
  avformat_close_input(&formatContext);

  // Cleanup FFmpeg
  avformat_network_deinit();
}
