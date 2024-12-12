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

inline bool is_base64(unsigned char c) {
  return std::isalnum(c) || (c == '+') || (c == '/');
}

std::string base64_decode(const std::string& encoded) {
  std::string decoded;
  int val = 0;
  int valb = -8;
  for(unsigned char c : encoded) {
    if(!is_base64(c) && c != '=') {
      return decoded;
    }
    if(c == '=') break;
    val = (val << 6) + base64_chars.find(c);
    valb += 6;
    if(valb >= 0) {
      decoded.push_back((val >> valb) & 0xFF);
      valb -= 8;
    }
  }
  return decoded;
}

