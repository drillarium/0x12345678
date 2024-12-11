#pragma once

class WriterBase {
public:
  virtual ~WriterBase() = default;
  virtual bool open() = 0;
  virtual bool close() = 0;
  virtual int write(const unsigned char *_packet, int _packetSize) = 0;
};
