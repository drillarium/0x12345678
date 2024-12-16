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
    std::cerr << "Usage: " << argv[0] << " <server_ip> <server_port>" << std::endl;
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

  RenderParser render;
  UDPReader reader(&render, argv[1], std::stoi(argv[2]));
  reader.open();

  while(1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  reader.close();

  IMG_Quit();
  SDL_Quit();

#ifdef _WIN32
  cleanup_socket_library(); // Cleanup for Windows
#endif

  return 0;
}
