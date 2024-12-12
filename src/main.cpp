#include <iostream>
#include <string>
#include <chrono>
#include <thread>

// For Windows
#ifdef _WIN32
  #include <winsock2.h>
  #include <Ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
#endif

#include "essence_block.h"
#include "queue_thread_safe.h"
#include "ffmpeg_producer.h"
#include "udp_writer.h"
#include "muxer_consumer.h"
#include "smt_producer.h"
#include "render_parser.h"
#include "udp_reader.h"

// Initialize sockets (Windows specific)
#ifdef _WIN32
void init_socket_library() {
  WSADATA wsaData;
  if(WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    std::cerr << "Winsock initialization failed" << std::endl;
    exit(1);
  }
}
#endif

// Cleanup sockets (Windows specific)
#ifdef _WIN32
void cleanup_socket_library() {
  WSACleanup();
}
#endif

int main(int argc, char *argv[]) {
  if(argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <input_file> <server_ip> <server_port> <bitrate>" << std::endl;
    return -1;
  }

#ifdef _WIN32
  init_socket_library(); // Initialize for Windows
#endif

  if(SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
    return -1;
  }

  if(!(IMG_Init(IMG_INIT_JPG) & IMG_INIT_JPG)) {
    std::cerr << "IMG_Init Error: " << IMG_GetError() << std::endl;
    SDL_Quit();
    return -1;
  }

  UDPWriter udpWriter(argv[2], std::stoi(argv[3]));
  ThreadSafeQueue<EssenceBlock *> queue;
  FFMPEGProducerParams producerParams0 = { 0 };
  std::thread producerThread0(ffmpeg_producer, std::ref(producerParams0), std::ref(queue), argv[1]);
  FFMPEGProducerParams producerParams1 = { 1 };
  std::thread producerThread1(ffmpeg_producer, std::ref(producerParams1), std::ref(queue), argv[1]);
  MuxerParams muxerParams;
  std::thread consumerThread(muxer_consumer, std::ref(muxerParams), std::ref(queue), std::ref(udpWriter));
  std::thread dummySMTThread(smt_producer, std::ref(queue));
  RenderParser render;
  UDPReader reader(&render, argv[2], std::stoi(argv[3]));
  reader.open();

  producerThread0.join();
  producerThread1.join();
  consumerThread.join();
  dummySMTThread.join();
  reader.close();

  IMG_Quit();
  SDL_Quit();

#ifdef _WIN32
  cleanup_socket_library(); // Cleanup for Windows
#endif

  return 0;
}
