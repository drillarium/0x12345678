#pragma once

#include "essence_block.h"

class ParserBase {
public:
  virtual ~ParserBase() = default;
  virtual int parse(EssenceBlock *_block) = 0;
};
