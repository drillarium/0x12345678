#pragma once

#include <stdint.h>
#include <string>

// Define the sync value (magic number)
// This should be a unique constant that will be easily recognizable by the receiver
#define SYNC_MAGIC_NUMBER 0x78563412

// Essence types, extends AVMediaType
enum EssenceType {
  ESSENCE_TYPE_UNKNOWN = 0xff,
  ESSENCE_TYPE_ED = 0,         // Essence data
  ESSENCE_TYPE_NULL = 1,       // Null packet
  ESSENCE_TYPE_SMT = 2,        // Stream Manipulation Table
  ESSENCE_TYPE_EA = 3,         // Essence announcement
};

#pragma pack(push, 1) 

// Define the block structure for streaming
struct EssenceBlock {
  uint32_t sync;          // SYNC_MAGIC_NUMBER
  uint32_t size;          // entire block size
  uint8_t essence_type;   // EssenceType
  uint8_t program_index;  // program index
  uint8_t stream_type;    // stream type (audio, video, subtitle, etc.)
  uint8_t stream_index;   // Which stream this block belongs to
  uint64_t timestamp;     // Timestamp of the block
  uint32_t payload_size;  // Size of the payload
};

#pragma pack(pop)

// Alloc essence block
__inline EssenceBlock* createEssenceBlock(int _payloadSize) {
  EssenceBlock* essenceBlock = (EssenceBlock*) (new uint8_t[_payloadSize + sizeof(EssenceBlock)]);
  essenceBlock->sync = SYNC_MAGIC_NUMBER;
  essenceBlock->size = sizeof(EssenceBlock);
  uint8_t* payload = (uint8_t*)(essenceBlock + 1);
  memset(payload, 0, _payloadSize);

  return essenceBlock;
}

__inline EssenceBlock * cloneEssenceBlock(EssenceBlock *_block) {
  EssenceBlock *block = createEssenceBlock(_block->payload_size);
  memcpy(block, _block, _block->size + _block->payload_size);
  return block;
}

__inline void destroyEssenceBlock(EssenceBlock **_block) {
  if(*_block) {
    delete[] * _block;
  }
  *_block = nullptr;
}
