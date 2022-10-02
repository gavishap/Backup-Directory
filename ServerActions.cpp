#include "ServerActions.h"
#include "ServerRequest.h"

bool ServerActions::fileBackup(const ServerRequest::Request& request, ServerResponse::Response*& response, boost::asio::ip::tcp::socket& sock, const std::string filepath, std::stringstream& err, std::string parsedFileName, uint8_t buffer[PACKET_SIZE])
{
	std::fstream fs;
	if (!_fileHandler.fileOpen(filepath, fs, true))
	{
		err << "user ID #" << +request.header.m_userID << ": File " << parsedFileName << " failed to open." << std::endl;
		return false;
	}
	uint32_t bytes = (PACKET_SIZE - request.sizeWithoutPayload());
	if (request.payload.m_size < bytes)
		bytes = request.payload.m_size;
	if (!_fileHandler.fileWrite(fs, request.payload.m_payload, bytes))
	{
		err << "user ID #" << +request.header.m_userID << ": Write to file " << parsedFileName << " failed." << std::endl;
		fs.close();
		return false;
	}

	while (bytes < request.payload.m_size)
	{
		if (!_socketHandler.receive(sock, buffer))
		{
			err << "user ID #" << +request.header.m_userID << ": receive file data from socket failed." << std::endl;
			fs.close();
			return false;
		}
		uint32_t length = PACKET_SIZE;
		if (bytes + PACKET_SIZE > request.payload.m_size)
			length = request.payload.m_size - bytes;
		if (!_fileHandler.fileWrite(fs, buffer, length))
		{
			err << "user ID #" << +request.header.m_userID << ": Write to file " << parsedFileName << " failed." << std::endl;
			fs.close();
			return false;
		}
		bytes += length;
	}
	fs.close();
	response->status = ServerResponse::Response::SUCCESS_BACKUP_DELETE;
	return true;
}





bool ServerActions::fileRestore(const ServerRequest::Request& request, ServerResponse::Response*& response, bool& responseSent, boost::asio::ip::tcp::socket& sock, const std::string filepath, std::stringstream& err, std::string parsedFileName, uint8_t buffer[PACKET_SIZE])
{
	std::fstream fs;
	if (!_fileHandler.fileOpen(filepath, fs))
	{
		err << "user ID #" << +request.header.m_userID << ": File " << parsedFileName << " failed to open." << std::endl;
		return false;
	}
	uint32_t fileSize = _fileHandler.fileSize(fs);
	if (fileSize == 0)
	{
		err << "user ID #" << +request.header.m_userID << ": File " << parsedFileName << " has 0 zero." << std::endl;
		fs.close();
		return false;
	}
	response->payload.m_size = fileSize;
	uint32_t bytes = (PACKET_SIZE - response->sizeWithoutPayload());
	response->payload.m_payload = new uint8_t[bytes];
	if (!_fileHandler.fileRead(fs, response->payload.m_payload, bytes))
	{
		err << "user ID #" << +request.header.m_userID << ": File " << parsedFileName << " reading failed." << std::endl;
		fs.close();
		return false;
	}

	// send first packet
	responseSent = true;
	response->status = ServerResponse::Response::SUCCESS_RESTORE;
	serializeResponse(*response, buffer);
	if (!_socketHandler.send(sock, buffer))
	{
		err << "Response sending on socket failed! user ID #" << +request.header.userId << std::endl;
		fs.close();
		sock.close();
		return false;
	}

	while (bytes < fileSize)
	{
		if (!_fileHandler.fileRead(fs, buffer, PACKET_SIZE) || !_socketHandler.send(sock, buffer))
		{
			err << "Payload data failure for user ID #" << +request.header.m_userID << std::endl;
			fs.close();
			sock.close();
			return false;
		}
		bytes += PACKET_SIZE;
	}

	destroy(response);
	fs.close();
	sock.close();
	return true;
}


bool ServerActions::fileList(const ServerRequest::Request& request, ServerResponse::Response*& response, bool& responseSent, boost::asio::ip::tcp::socket& sock, const std::string filepath, std::stringstream& err, std::string parsedFileName, uint8_t buffer[PACKET_SIZE], std::stringstream& userPathSS)
{
	std::set<std::string> userFiles;
	std::string userFolder(userPathSS.str());
	if (!_fileHandler.getFilesList(userFolder, userFiles))
	{
		err << "Request Error for user ID #" << +request.header.userId << ": FILE_DIR generic failure." << std::endl;
		response->status = Response::ERROR_GENERIC;  // can be only generic error. empty files were validated before.
		return false;
	}
	const size_t filenameLen = 32;  // random string length, as required.
	response->filename = new uint8_t[filenameLen];
	response->nameLen = filenameLen;
	memcpy(response->filename, randString(filenameLen).c_str(), filenameLen);
	response->status = SResponse::SUCCESS_DIR;

	// list's size calculation.
	size_t listSize = 0;
	for (const auto& fn : userFiles)
		listSize += fn.size() + 1;  // +1 for '\n' to represent filename ending.
	response->payload.size = listSize;
	auto const listPtr = new uint8_t[listSize];           // assumption: listSize will not exceed RAM. (mentioned in forum).
	auto ptr = listPtr;
	for (const auto& fn : userFiles)
	{
		memcpy(ptr, fn.c_str(), fn.size());
		ptr += fn.size();
		*ptr = '\n';
		ptr += 1;
	}
	if (response->sizeWithoutPayload() + listSize <= PACKET_SIZE)  // file names do not exceed PACKET_SIZE.
	{
		response->payload.payload = listPtr;  // will be de-allocated by outer logic.
		return true;
	}

	// file names exceed PACKET_SIZE. Split Message.
	ptr = listPtr;
	responseSent = true;  // specific sending logic. no need to send after function end.
	uint32_t bytes = PACKET_SIZE - response->sizeWithoutPayload();  // leftover bytes
	response->payload.payload = new uint8_t[bytes];
	memcpy(response->payload.payload, ptr, bytes);
	ptr += bytes;

	// send first packet
	serializeResponse(*response, buffer);
	if (!_socketHandler.send(sock, buffer))
	{
		err << "Response sending on socket failed! user ID #" << +request.header.userId << std::endl;
		destroy(response);
		sock.close();
		return false;
	}

	bytes = PACKET_SIZE;  // bytes sent.
	while (bytes < (response->sizeWithoutPayload() + listSize))
	{
		memcpy(buffer, ptr, PACKET_SIZE);
		ptr += PACKET_SIZE;
		bytes += PACKET_SIZE;
		if (!_socketHandler.send(sock, buffer))
		{
			err << "Payload data failure for user ID #" << +request.header.userId << std::endl;
			destroy(response);
			sock.close();
			return false;
		}
	}

	destroy(response);
	sock.close();
	return true;
};
	
