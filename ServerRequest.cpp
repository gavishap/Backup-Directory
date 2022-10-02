
#include <sstream> 
#include <algorithm>
#include <fstream>
#include <thread>
#include "ServerResponse.h"

#define PACKET_SIZE  1024

/**
   @brief generate a random string of given length.
		  based on https://stackoverflow.com/questions/440133/how-do-i-create-a-random-alpha-numeric-string-in-c
   @param length the string's length to generate.
   @return the generated string.
 */




namespace ServerRequestFuncs {


	Request* deserializeRequest(const uint8_t* buffer, const uint32_t size)
	{
		uint32_t bytesRead = 0;
		const uint8_t* ptr = buffer;
		if (size < sizeof(Request::RequestHeader))
			return nullptr; // invalid minimal size.

		auto const request = new Request;

		// Fill minimal header
		memcpy(request, ptr, sizeof(Request::RequestHeader));
		bytesRead += sizeof(Request::RequestHeader);
		ptr += sizeof(Request::RequestHeader);
		if (bytesRead + sizeof(uint16_t) > size)
			return request;  // return the request with minimal header.

		// Copy name length
		memcpy(&(request->nameLen), ptr, sizeof(uint16_t));
		bytesRead += sizeof(uint16_t);
		ptr += sizeof(uint16_t);
		if ((request->nameLen == 0) || ((bytesRead + request->nameLen) > size))
			return request;  // name length invalid.

		request->filename = new uint8_t[request->nameLen + 1];
		memcpy(request->filename, ptr, request->nameLen);
		request->filename[request->nameLen] = '\0';
		bytesRead += request->nameLen;
		ptr += request->nameLen;

		if (bytesRead + sizeof(uint32_t) > size)
			return request;

		// copy payload size
		memcpy(&(request->payload.m_size), ptr, sizeof(uint32_t));
		bytesRead += sizeof(uint32_t);
		ptr += sizeof(uint32_t);
		if (request->payload.m_size == 0)
			return request;  // name length invalid.

		// copy payload until size limit.
		uint32_t leftover = size - bytesRead;
		if (request->payload.m_size < leftover)
			leftover = request->payload.m_size;
		request->payload.m_payload = new uint8_t[leftover];
		memcpy(request->payload.m_payload, ptr, leftover);

		return request;
	}

	bool lock(const Request& request, std::map<uint32_t, std::atomic<bool>> userHandling)
	{
		if (request.header.m_userID == 0)
			return true;
		if (userHandling[request.header.m_userID])
			return false;
		userHandling[request.header.m_userID] = true;
		return true;
	}

	std::string randString(const uint32_t length)
	{
		if (length == 0)
			return "";
		auto randChar = []() -> char
		{
			const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
			const size_t maxIndex = (sizeof(charset) - 1);
			return charset[rand() % maxIndex];
		};
		std::string str(length, 0);
		std::generate_n(str.begin(), length, randChar);
		return str;
	}




	bool handleRequest(const Request& request, ServerResponse::Response*& response, bool& responseSent, boost::asio::ip::tcp::socket& sock, std::stringstream& err)
	{
		responseSent = false;
		response = new ServerResponse::Response;
		if (request.header.m_userID == 0) // invalid ID.
		{
			err << "Invalid User ID #" << +request.header.m_userID << std::endl;
			response->status = ServerResponse::Response::ERROR_GENERIC;
			return false;
		}

		// Common validation for FILE_RESTORE | FILE_REMOVE | FILE_DIR requests.
		if ((request.header.m_op & (Request::EOp::CLI_FILE_RESTORE | Request::EOp::CLI_FILE_REMOVE | Request::EOp::CLI_FILE_LIST)) == request.header.m_op)
		{
			if (!userHasFiles(request.header.m_userID))
			{
				err << "User #" << +request.header.m_userID << " has no files!" << std::endl;
				response->status = ServerResponse::Response::ERROR_NO_FILES;
				return false;
			}
		}

		// Common validation for FILE_BACKUP | FILE_RESTORE | FILE_REMOVE requests.
		std::string parsedFileName; // will be used as parsed filename string.
		if ((request.header.m_op & (Request::EOp::CLI_FILE_BACKUP | Request::EOp::CLI_FILE_RESTORE | Request::EOp::CLI_FILE_REMOVE)) == request.header.m_op)
		{
			if (!parseFilename(request.nameLen, request.filename, parsedFileName))
			{
				err << "Request Error for user ID #" << +request.header.m_userID << ": Invalid filename!" << std::endl;
				response->status = ServerResponse::Response::ERROR_GENERIC;
				return false;
			}
			copyFilename(request, *response);
		}

		std::stringstream userPathSS;
		std::stringstream filepathSS;
		userPathSS << BACKUP_FOLDER << request.header.m_userID << "/";
		filepathSS << userPathSS.str() << parsedFileName;
		const std::string filepath = filepathSS.str();

		// Common validation for FILE_RESTORE | FILE_REMOVE requests.
		if ((request.header.m_op & (Request::EOp::CLI_FILE_RESTORE | Request::EOp::CLI_FILE_REMOVE)) == request.header.m_op)
		{
			if (!FileManager::fileExists(filepath))
			{
				err << "Request Error for user ID #" << +request.header.m_userID << ": File not exists!" << std::endl;
				response->status = ServerResponse::Response::ERROR_NOT_EXIST;
				return false;
			}
		}

		// Specifics
		response->status = ServerResponse::Response::ERROR_GENERIC;  // until proven otherwise..
		uint8_t buffer[PACKET_SIZE];
		switch (request.header.m_op)
		{
			/**
			   save file to disk. do not close socket on failure. response handled outside.
			 */
		case Request::EOp::CLI_FILE_BACKUP:
		{
			return ServerActions::fileBackup(request, response, sock, filepath, err, parsedFileName, buffer);
		}

		/**
		   Restore file from disk. close socket on failure. specific socket logic.
		 */
		case Request::EOp::CLI_FILE_RESTORE:
		{
			return ServerActions::fileRestore(request, response, responseSent, sock, filepath, err, parsedFileName, buffer);
		}

		/**
		   Remove file from disk. response handled outside.
		 */
		case Request::EOp::CLI_FILE_REMOVE:
		{
			if (!FileManager::fileRemove(filepath))
			{
				err << "Request Error for user ID #" << +request.header.m_userID << ": File deletion failed!" << std::endl;
				return false;
			}
			response->status = ServerResponse::Response::SUCCESS_BACKUP_DELETE;
			return true;
		}


		/**
		   Read file list from disk, separate to packets if file names size exceeding PACKET_SIZE, send to client.
		   Specific socket logic. close socket on failure.
		*/
		case Request::EOp::CLI_FILE_LIST:
		{
			return ServerActions::fileList(request, response, responseSent, sock, filepath, err, parsedFileName, buffer, userPathSS);

		}
		default:  // response handled outside.
		{
			err << "Request Error for user ID #" << +request.header.m_userID << ": Invalid request code: " << +request.header.m_op << std::endl;
			return true;
		}
		// end of switch
		} // end of resolve()
	}


	void destroy(Request* request)
	{
		if (request != nullptr)
		{
			destroy_ptr(request->filename);
			destroy_ptr(request->payload.m_payload);
			delete request;
			request = nullptr;
		}
	}




	void unlock(const Request& request , std::map<uint32_t, std::atomic<bool>> userHandling)
	{
		if (request.header.m_userID == 0)
			return;
		userHandling[request.header.m_userID] = false;
	}

}
