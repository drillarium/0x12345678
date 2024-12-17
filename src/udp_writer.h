#pragma once

#include <iostream>
#ifdef _WIN32
  #include <winsock2.h>
  #include <Ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
#endif
#include "writer_base.h"

// UDP writer
class UDPWriter : public WriterBase {
public:
  UDPWriter(const char *_server, int _port)
  :server_(_server)
  ,port_(_port)
  {
  }

  bool open() {
    // Create socket (both Linux and Windows)
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd_ < 0) {
      std::cerr << "Error creating socket" << std::endl;
      return false;
    }

    // Set socket to non-blocking mode
    u_long mode = 1;  // 1 to enable non-blocking mode, 0 to disable
    if(ioctlsocket(sockfd_, FIONBIO, &mode) != 0) {
      std::cerr << "Failed to set non-blocking mode: " << WSAGetLastError() << std::endl;
      return false;
    }

    // Enable SO_REUSEADDR to allow multiple processes to use the same port
    BOOL reuse = TRUE;
    if(setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
      std::cerr << "setsockopt SO_REUSEADDR failed with error: " << WSAGetLastError() << std::endl;
      return false;
    }

    // Setup the server address
    memset(&serverAddr_, 0, sizeof(serverAddr_));
    serverAddr_.sin_family = AF_INET;
    serverAddr_.sin_port = htons(port_);

    // Convert IP address from text to binary format
    if(inet_pton(AF_INET, server_.c_str(), &serverAddr_.sin_addr) <= 0) {
      std::cerr << "Invalid IP address" << std::endl;
      return false;
    }

    // Increase the size of the socket's send buffer
    int sendBufferSize = 1048576;
    if(setsockopt(sockfd_, SOL_SOCKET, SO_SNDBUF, (const char*)&sendBufferSize, sizeof(sendBufferSize)) < 0) {
      std::cerr << "setsockopt SO_SNDBUF failed" << std::endl;
      return false;
    }

    return true;
  }

  bool close() {
    // Close the socket
#ifdef _WIN32
    closesocket(sockfd_);  // On Windows
#else 
    close(sockfd_);        // On Linux
#endif

    return true;
  }

  #define CHUNK_SIZE 1024 

  int write(const unsigned char *_packet, int _packetSize) {
    const uint8_t* message = _packet;
    size_t bytesSent = 0;
    int numPackets = 0;

    while(bytesSent < _packetSize) {
      size_t remaining = _packetSize - bytesSent;
      size_t chunkSize = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;

      // Send the message
      int sentBytes = sendto(sockfd_, (const char*) message + bytesSent, (int) chunkSize, 0, (struct sockaddr*) &serverAddr_, sizeof(serverAddr_));
      if(sentBytes == SOCKET_ERROR) {
        // never returns EWOULDBLOCK after setup socket to non-nlocking. I'm forcing to send huge blocks
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Wait briefly and retry
          continue;
        }
        else {
          std::cerr << "Error sending message " << WSAGetLastError() << " " << chunkSize << std::endl;
          return false;
        }
      }
      bytesSent += sentBytes;

      // Problems sending PNG into SMT. Tests sleeping because EWOULDBLOCK never is returned case send buffer is full
      numPackets++;
      if(numPackets % 50 == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }

    return (int) bytesSent;
  }

protected:
  std::string server_;
  int port_ = 0;
#ifdef _WIN32
  SOCKET sockfd_ = 0;
#else
  int sockfd_ = 0;
#endif
  struct sockaddr_in serverAddr_ = {};
};
