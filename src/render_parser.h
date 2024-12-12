#pragma once

#include "parser_base.h"
#include <nlohmann/json.hpp>
extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libswscale/swscale.h>
  #include <libavutil/imgutils.h>
}

#include <SDL.h>
#include <SDL_image.h>
#pragma comment(lib, "SDL2.lib")
#pragma comment(lib, "SDL2main.lib")
#pragma comment(lib, "SDL2_image.lib")

class RenderParser : public ParserBase {
public:
  RenderParser() {

  }

  ~RenderParser() {
    destroyEssenceBlock(&EABlock_);
    avcodec_free_context(&videoCodecCtx_); 
    if(swsCtx_) {
      sws_freeContext(swsCtx_);
    }
    if(window_) {
      SDL_DestroyWindow(window_);
    }
    if(texture_) {
    SDL_DestroyTexture(texture_);
    }
    if(renderer_) {
      SDL_DestroyRenderer(renderer_);
    }
    if(imageTexture_) {
      SDL_DestroyTexture(imageTexture_);
    }
  }

  int parse(EssenceBlock* _block) {
    // essence data
    if(_block->essence_type == EssenceType::ESSENCE_TYPE_ED) {
      if(EABlock_) {
        int programIndex = programIndex_;
        if(programIndex == _block->program_index) {
          int streamIndex = _block->stream_index;
          if(streamIndex == videoStreamIndex_) {
            if(videoCodecCtx_) {
              AVPacket *packet = av_packet_alloc();
              packet->data = (uint8_t *) (_block + 1);
              packet->size = _block->payload_size;

              if(avcodec_send_packet(videoCodecCtx_, packet) >= 0) {
                AVFrame *frame = av_frame_alloc();
                while(avcodec_receive_frame(videoCodecCtx_, frame) == 0) {
                  std::cout << "Decoded frame: " << frame->pts << std::endl;

                  if(!swsCtx_) {
                    swsCtx_ = sws_getContext(videoCodecCtx_->width, videoCodecCtx_->height, videoCodecCtx_->pix_fmt, videoCodecCtx_->width, videoCodecCtx_->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
                  }

                  if(!window_) {
                    // Create SDL window and renderer
                    window_ = SDL_CreateWindow("H.264 Decoder", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, videoCodecCtx_->width, videoCodecCtx_->height, SDL_WINDOW_SHOWN);
                    if(window_) {
                      renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
                      texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, videoCodecCtx_->width, videoCodecCtx_->height);
                    }
                  }

                  // Convert the frame to YUV420P
                  AVFrame *frameYUV = av_frame_alloc();
                  uint8_t *buffer = (uint8_t *) av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, videoCodecCtx_->width, videoCodecCtx_->height, 1));
                  av_image_fill_arrays(frameYUV->data, frameYUV->linesize, buffer, AV_PIX_FMT_YUV420P, videoCodecCtx_->width, videoCodecCtx_->height, 1);
                  sws_scale(swsCtx_, frame->data, frame->linesize, 0, videoCodecCtx_->height, frameYUV->data, frameYUV->linesize);

                  // Update the SDL texture
                  SDL_UpdateYUVTexture(texture_, nullptr, frameYUV->data[0], frameYUV->linesize[0], frameYUV->data[1], frameYUV->linesize[1], frameYUV->data[2], frameYUV->linesize[2]);

                  // Render the frame
                  SDL_RenderClear(renderer_);
                  SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);

                  // overlay image
                  if(imageTexture_) {
                    SDL_Rect overlayRect = { 100, 100, 200, 150 };
                    SDL_RenderCopy(renderer_, imageTexture_, nullptr, &overlayRect);
                  }

                  SDL_RenderPresent(renderer_);

                  SDL_Event e;
                  SDL_PollEvent(&e);                  

                  av_free(buffer);
                  av_frame_free(&frameYUV);
                  
                  av_frame_unref(frame);                
                }
                av_frame_free(&frame);
              }
            }
          }
          else if(streamIndex == audioStreamIndex_) {

          }
        }
      }
    }
    // NULL packet
    else if(_block->essence_type == EssenceType::ESSENCE_TYPE_NULL) {
      // NOOP
    }
    // SMT table (Stream manipulation table)
    else if(_block->essence_type == EssenceType::ESSENCE_TYPE_SMT) {
      const uint8_t* payload = (const uint8_t*)(_block + 1);
      std::string receivedData((const char*)payload, _block->payload_size);
      nlohmann::json SMTJson = nlohmann::json::parse(receivedData);

      // Output the reconstructed JSON
      std::cout << "Reconstructed JSON:\n" << SMTJson.dump(4) << std::endl;   

      for(int i = 0; i < SMTJson["actions"].size(); i++) {
        std::string actionType = SMTJson["actions"][i]["action"];
        uint64_t id = SMTJson["actions"][i]["id"];
        if(actionType == ACTION_ADD_IMAGE) {
          std::string imageType = SMTJson["actions"][i]["data_type"];
          std::string base64Image = SMTJson["actions"][i]["data"];
          std::string image = base64_decode(base64Image);
          if(imageTexture_) {
            SDL_DestroyTexture(imageTexture_);
          }
          imageTexture_ = loadJPEGFromMemory(image, renderer_);
          actionId_ = id;
        }
        else if(actionType == ACTION_REMOVE_IMAGE) {
          if(id == actionId_) {
            if(imageTexture_) {
              SDL_DestroyTexture(imageTexture_);
            }
            imageTexture_ = nullptr;
          }
        }
      }
    }
    // Essence Announcement
    else if(_block->essence_type == EssenceType::ESSENCE_TYPE_EA) {
      // first payload received
      if(!EABlock_) {
        EABlock_ = cloneEssenceBlock(_block);
        const uint8_t* payload = (const uint8_t *) (EABlock_ + 1);
        std::string receivedData((const char *) payload, EABlock_->payload_size);
        EAPayload_ = nlohmann::json::parse(receivedData);
        // Output the reconstructed JSON
        std::cout << "Reconstructed JSON:\n" << EAPayload_.dump(4) << std::endl;
        programIndex_ = EAPayload_["program_index"];
        for(int i = 0; i < EAPayload_["streams"].size(); i++) {
          std::string type = EAPayload_["streams"][i]["type"];
          if( (videoStreamIndex_ < 0) && (type == "video") ) {
            videoStreamIndex_ = EAPayload_["streams"][i]["index"];

            // open decoder
            std::string codecName = EAPayload_["streams"][i]["codec"];
            const AVCodec *codec = avcodec_find_decoder_by_name(codecName.c_str());
            if(codec) {   
              videoCodecCtx_ = avcodec_alloc_context3(codec);
              if(videoCodecCtx_) {
                if(avcodec_open2(videoCodecCtx_, codec, nullptr) < 0) {
                  avcodec_free_context(&videoCodecCtx_);
                }                              
              }
            }
          }
          else if((audioStreamIndex_ < 0) && (type == "audio")) {
            audioStreamIndex_ = EAPayload_["streams"][i]["index"];
          }
        }
      }
    }

    return _block->size + _block->payload_size;
  }

  protected:
    // Load JPEG from memory into an SDL_Texture
    SDL_Texture* loadJPEGFromMemory(const std::string &jpegData, SDL_Renderer *renderer) {
      SDL_RWops* rw = SDL_RWFromConstMem(jpegData.data(), (int) jpegData.size());
      if(!rw) {
        std::cerr << "SDL_RWFromConstMem Error: " << SDL_GetError() << std::endl;
        return nullptr;
      }

      SDL_Surface* surface = IMG_Load_RW(rw, 1);
      if(!surface) {
        std::cerr << "IMG_Load_RW Error: " << IMG_GetError() << std::endl;
        return nullptr;
      }

      SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
      SDL_FreeSurface(surface);

      return texture;
    }

  protected:
    EssenceBlock *EABlock_ = nullptr;
    nlohmann::json EAPayload_;
    int programIndex_ = -1;
    int videoStreamIndex_ = -1;
    int audioStreamIndex_ = -1;
    AVCodecContext *videoCodecCtx_ = nullptr;
    SwsContext *swsCtx_ = nullptr;
    SDL_Window *window_ = nullptr;
    SDL_Texture *texture_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *imageTexture_ = nullptr;
    uint64_t actionId_ = -1;
};
