/**
  Maman 14
  @CSocketHandler handle a boost asio socket. e.g. read/write.
  @author Roman Koifman
 */

#pragma once
#include <boost/asio/ip/tcp.hpp>
#include "ServerRequest.cpp"

class CommunicationHandler
{
    #define PACKET_SIZE  1024
 

public:
    bool receive(boost::asio::ip::tcp::socket& sock, uint8_t* (buffer)[PACKET_SIZE]);
    bool send(boost::asio::ip::tcp::socket& sock,  uint8_t* (buffer)[PACKET_SIZE]);
    bool handleSocketFromThread(boost::asio::ip::tcp::socket& sock, std::stringstream& err);

    
   
};


struct Payload  // Common for Request & Response.
{
    uint32_t m_size;     // payload size
    uint8_t* m_payload;
    Payload() : m_size(0), m_payload(nullptr) {}
};

