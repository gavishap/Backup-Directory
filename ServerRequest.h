#pragma once
#include "FileManager.h"
#include <map>
#include <atomic>
#include <boost/asio/ip/tcp.hpp>
#include "CommunicationHandler.h"
#include "ServerResponse.h"
#include <fstream>
#include <thread>
#include "ServerActions.h"
#define PACKET_SIZE  1024
//#include "ServerActions.h"


	struct Request
	{
		#pragma pack(push, 1)      // SRequestHeader is copied to buffer to send on socket. Hence, should be aligned to 1.
		struct RequestHeader
		{
			uint32_t m_userID;     // User ID
			uint8_t  m_version;    // Client Version
			uint8_t  m_op;         // Request Code
			RequestHeader() : m_userID(0), m_version(0), m_op(0) {}
		};
		#pragma pack(pop)

		enum EOp
		{
			CLI_FILE_BACKUP = 100,  // Save file backup. All fields should be valid.
			CLI_FILE_RESTORE = 200,  // Restore a file. size, payload unused.
			CLI_FILE_REMOVE = 201,  // Delete a file. size, payload unused.
			CLI_FILE_LIST = 202   // List all client's files. name_len, filename, size, payload unused.
		};

		RequestHeader header;  // request header
		uint16_t nameLen;       // FileName length
		uint8_t* filename;      // FileName
		Payload payload;
		Request() : nameLen(0), filename(nullptr) {}
		uint32_t sizeWithoutPayload() const
		{
			return (sizeof(header) + sizeof(nameLen) + nameLen + sizeof(payload.m_size));
		}


	};

	Request* deserializeRequest(const uint8_t* buffer, const uint32_t size);


class ServerRequest :public ServerResponse
    {
    public:

#define SERVER_VERSION 1  // Shouldn't be verified. Requirement from forum.


       

        std::map<uint32_t, std::atomic<bool>> _userHandling;         // indicates whether working on user's request
        std::string randString(const uint32_t length) const;
        bool userHasFiles(const uint32_t userID);
        



        

        void unlock(const Request& request);
    public:

    };