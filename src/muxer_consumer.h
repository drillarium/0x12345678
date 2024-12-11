#pragma once

#include "essence_block.h"
#include "queue_thread_safe.h"
#include "muxer_timestamp.h"
#include <iostream>
#include <map>

struct MuxerParams {
  int bitrate = 8000000;
  int checkBitratePeriodMs = 100;
  int writeEAPeriodMs = 50;
};

#define NULL_PAYLOAD_SIZE 1024 * 2

void muxer_consumer(MuxerParams &_params, ThreadSafeQueue<EssenceBlock *> &_queue, WriterBase &_writer) {
  // open writer
  _writer.open();

  // null packet
  EssenceBlock *NULLBlock = createEssenceBlock(NULL_PAYLOAD_SIZE);
  NULLBlock->essence_type = EssenceType::ESSENCE_TYPE_NULL;
  NULLBlock->program_index = 0xff;
  NULLBlock->stream_index = 0xff;
  NULLBlock->stream_type = 0xff;  
  NULLBlock->timestamp = 0;
  NULLBlock->payload_size = NULL_PAYLOAD_SIZE;

  // start clock (bitrate)
  auto startTimeBitrate = std::chrono::steady_clock::now();
  uint64_t totalBytesSent = 0;
  uint64_t targetBitrate = _params.bitrate;

  // timestamp
  MuxerTimestamp mts;
  mts.start();

  // Essence Announcement
  std::map<int, EssenceBlock *> eaBlocks;
  auto startTimeEA = std::chrono::steady_clock::now();

  while(true) {
    if(!_queue.empty()) {
      EssenceBlock *block = _queue.pop();
      block->timestamp = mts.getCurrentTimestamp();

      // write
      int bytesSent = _writer.write((const uint8_t *) block, block->size + block->payload_size);
      totalBytesSent += bytesSent;

      // check ESSENCE_TYPE_EA blocks and clone them. Insert every x ms
      if(block->essence_type == EssenceType::ESSENCE_TYPE_EA) {
        int programIndex = block->program_index;
        if(eaBlocks.find(programIndex) != eaBlocks.end()) {
          EssenceBlock *block = eaBlocks[programIndex];
          destroyEssenceBlock(&block);
        }
        eaBlocks[programIndex] = cloneEssenceBlock(block);
      }

      destroyEssenceBlock(&block);
    }
    else {
      auto now = std::chrono::steady_clock::now();

      // CBR
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTimeBitrate).count();
      if(elapsed >= _params.checkBitratePeriodMs) {
        double currentBitrate = (totalBytesSent * 8.0) / (elapsed / 1000.0); // bps

        if(currentBitrate < targetBitrate) {
          size_t nullPacketsNeeded = (size_t) (((targetBitrate - currentBitrate) / 8.0) * (elapsed / 1000.0) / (NULLBlock->size + NULLBlock->payload_size));

          for(size_t i = 0; i < nullPacketsNeeded && _queue.empty(); ++i) {
            // write
            NULLBlock->timestamp = mts.getCurrentTimestamp();
            _writer.write((const uint8_t *) NULLBlock, NULLBlock->size + NULLBlock->payload_size);
          }
        }

        // Reset counters
        totalBytesSent = 0;
        startTimeBitrate = now;
      }

      // Essence announcement
      elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTimeEA).count();
      if(elapsed >= _params.writeEAPeriodMs) {
        for(auto& pair : eaBlocks) {
          EssenceBlock *block = pair.second;
          block->timestamp = mts.getCurrentTimestamp();
          _writer.write((const uint8_t *) block, block->size + block->payload_size);
        }
        startTimeEA = now;
      }
    }
  }

  // Close writer
  _writer.close();

  // NULL
  destroyEssenceBlock(&NULLBlock);

  // EA
  for(auto& pair : eaBlocks) {
    EssenceBlock *block = pair.second;
    destroyEssenceBlock(&block);
  }
  eaBlocks.clear();
}
