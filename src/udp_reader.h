#pragma once

#include <iostream>
#ifdef _WIN32
  #include <winsock2.h>
  #include <Ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
#endif
#include <thread>
#include <array>
#include <vector>
#include <nlohmann/json.hpp>
#include "essence_block.h"
#include "smt_producer.h"
#include "base64_simple.h"

#define MAX_AV_PACKET_SIZE 1024 * 1024 * 4

// UDP reader
class UDPReader {
public:
  UDPReader(const char *_server, int _port)
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

    // Enable SO_REUSEADDR to allow multiple processes to use the same port
    BOOL reuse = TRUE;
    if(setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
      std::cerr << "setsockopt SO_REUSEADDR failed with error: " << WSAGetLastError() << std::endl;
      return false;
    }

    // Bind to the specified port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(sockfd_, (struct sockaddr*) &addr, sizeof(addr)) == SOCKET_ERROR) {
      std::cerr << "Failed to bind socket with error: " << WSAGetLastError() << std::endl;
      return false;
    }

    // Join the multicast group
    ip_mreq mreq{};
    // Convert IP address from text to binary format
    if(inet_pton(AF_INET, server_.c_str(), &mreq.imr_multiaddr.s_addr) <= 0) {
      std::cerr << "Invalid IP address" << std::endl;
      return false;
    }
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if(setsockopt(sockfd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) == SOCKET_ERROR) {
      std::cerr << "Failed to join multicast group with error: " << WSAGetLastError() << std::endl;
      return false;
    }

    stopFlag_ = false;
    workerThread_ = std::thread(&UDPReader::readerLoop, this);

    return true;
  }

  bool close() {
    stopFlag_ = true;
    if(workerThread_.joinable()) {
      workerThread_.join();
    }

    // Close the socket
#ifdef _WIN32
    closesocket(sockfd_);  // On Windows
#else 
    close(sockfd_);        // On Linux
#endif

    return true;
  }

protected:
  void readerLoop() { 
    std::array<uint8_t, 1500> buffer; // Typical UDP packet size
    size_t current_size = 0;

    while(!stopFlag_) {
      int received = recv(sockfd_, reinterpret_cast<char*>(buffer.data()), (int) buffer.size(), 0);
      if(received <= 0) {
        continue;
      }

      size_t i = 0;
      while(i < received) {
        accumulatedData_.push_back(buffer[i]);

        // Search for sync word
        if(accumulatedData_.size() >= 4) {
          uint32_t last_word = *reinterpret_cast<uint32_t*>(&accumulatedData_[accumulatedData_.size() - 4]);
          if(last_word == SYNC_MAGIC_NUMBER) {
            // Sync word found, process the block         
            if(accumulatedData_.size() >= sizeof(EssenceBlock)) {
              EssenceBlock *block = reinterpret_cast<EssenceBlock *>(accumulatedData_.data());
              processData(block);
              accumulatedData_.erase(accumulatedData_.begin(), accumulatedData_.end() - 4);
            }
          }
        }

        if(accumulatedData_.size() > MAX_AV_PACKET_SIZE) {
          std::cerr << "Buffer overflow prevented. Dropping data." << std::endl;
          accumulatedData_.clear();
        }

        ++i;
      }
    }
  }

  void processData(EssenceBlock *block) {
    // essence data
    if(block->essence_type == EssenceType::ESSENCE_TYPE_ED) {

    }
    // NULL packet
    else if(block->essence_type == EssenceType::ESSENCE_TYPE_NULL) {
      // NOOP
    }
    // SMT table (Stream manipulation table)
    else if(block->essence_type == EssenceType::ESSENCE_TYPE_SMT) {
      const uint8_t* payload = (const uint8_t *) (block + 1);
      std::string receivedData((const char *) payload, block->payload_size);
      nlohmann::json SMTJson = nlohmann::json::parse(receivedData);
      
      // Output the reconstructed JSON
      // std::cout << "Reconstructed JSON:\n" << SMTJson.dump(4) << std::endl;   

      for(int i = 0; SMTJson["actions"].size(); i++) {
        std::string actionType = SMTJson["actions"][i]["action"];
        if(actionType == ACTION_ADD_IMAGE) {
          std::string imageType = SMTJson["actions"][i]["data_type"];
          std::string base64Image = SMTJson["actions"][i]["data"];
          std::string image = base64_decode(base64Image);
        }
      }
    }
    // Essence Announcement
    else if(block->essence_type == EssenceType::ESSENCE_TYPE_EA) {
      const uint8_t* payload = (const uint8_t *) (block + 1);
      std::string receivedData((const char *) payload, block->payload_size);
      nlohmann::json EAJson = nlohmann::json::parse(receivedData);

      // Output the reconstructed JSON
      // std::cout << "Reconstructed JSON:\n" << EAJson.dump(4) << std::endl;
    }
  }

protected:
  std::string server_;
  int port_ = 0;
#ifdef _WIN32
  SOCKET sockfd_ = 0;
#else
  int sockfd_ = 0;
#endif
  std::thread workerThread_;
  std::atomic<bool> stopFlag_ = false;
  std::vector<uint8_t> accumulatedData_;
};
