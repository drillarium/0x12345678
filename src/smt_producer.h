#pragma once

#include "essence_block.h"
#include "queue_thread_safe.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include "base64_simple.h"

#define MAX_MANIPULATION_TABLE_PAYLOAD_SIZE 1024 * 1024

void setStreamManipulaitonTableBlock(EssenceBlock *_block, nlohmann::json &_jsonInfo) {
  std::string serialized_json = _jsonInfo.dump();
  std::vector<uint8_t> data(serialized_json.begin(), serialized_json.end());

  // Set block metadata
  _block->payload_size = (uint32_t) data.size();

  // payload
  uint8_t* payload = (uint8_t*)(_block + 1);
  std::memcpy(payload, &(data[0]), data.size());
}

#define ACTION_ADD_IMAGE "add_image"
#define ACTION_REMOVE_IMAGE "remove_image"

// Function to read binary file into a vector
std::vector<uint8_t> read_binary_file(const std::string& filepath) {
  std::ifstream file(filepath, std::ios::binary);
  if(!file) {
    throw std::runtime_error("Failed to open file: " + filepath);
  }
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}

void smt_producer(ThreadSafeQueue<EssenceBlock*>& _queue) {
  auto startTime = std::chrono::steady_clock::now();
  bool drawImage = true;
  uint64_t actionId = 1001;

  while(true) {
    // now
    auto now = std::chrono::steady_clock::now();
    // elapsed
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
    if(elapsed >= 5000) {
      // block
      EssenceBlock* block = createEssenceBlock(MAX_MANIPULATION_TABLE_PAYLOAD_SIZE);
      block->essence_type = EssenceType::ESSENCE_TYPE_SMT;
      block->program_index = 0;
      block->stream_type = 0xff;
      block->stream_index = 0xff;
      block->timestamp = 0;

      // build json
      nlohmann::json smt_info;

      if(drawImage) {
        // Path to the image
        std::string image_path = actionId%2==0? "example_image.jpg" : "example_image.png";

        // Read binary image data
        std::vector<uint8_t> image_data = read_binary_file(image_path);

        // Encode the image in Base64
        std::string base64_image = base64_encode(image_data);

        nlohmann::json action_json;
        action_json["action"] = ACTION_ADD_IMAGE;
        action_json["id"] = actionId;
        action_json["timestamp"] = 0; // ASAP
        action_json["data"] = base64_image;
        action_json["data_type"] = actionId%2==0? "image/jpeg" : "image/png";
        action_json["x_percentage"] = 10.0;
        action_json["y_percentage"] = 20.0;
        action_json["width_percentage"] = 15.0;
        action_json["height_percentage"] = 10.0;
        smt_info["actions"].push_back(action_json);
      }
      else {
        nlohmann::json action_json;
        action_json["action"] = ACTION_REMOVE_IMAGE;
        action_json["id"] = actionId++;
        action_json["timestamp"] = 0; // ASAP
        smt_info["actions"].push_back(action_json);
      }

      drawImage = !drawImage;
      setStreamManipulaitonTableBlock(block, smt_info);

      // register
      _queue.push(block);
      
      startTime = now;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}