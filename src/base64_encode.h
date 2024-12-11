#pragma once

#include <string>
#include <vector>

static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

std::string base64_encode(const std::vector<uint8_t>& data) {
  std::string encoded;
  size_t i = 0;
  uint32_t val = 0;
  int valb = -6;

  for(uint8_t c : data) {
    val = (val << 8) + c;
    valb += 8;
    while(valb >= 0) {
      encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if(valb > -6) {
    encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
  }
  while(encoded.size() % 4) {
    encoded.push_back('=');
  }
  return encoded;
}

