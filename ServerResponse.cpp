#include "ServerResponse.h"

namespace ServerResponseFuncs {

	void serializeResponse(const ServerResponse::Response& response, uint8_t* buffer)
	{
		uint8_t* ptr = buffer;
		uint32_t size = (PACKET_SIZE - response.sizeWithoutPayload());
		if (response.payload.m_size < size)
			size = response.payload.m_size;

		memcpy(ptr, &(response.version), sizeof(response.version));
		ptr += sizeof(response.version);
		memcpy(ptr, &(response.status), sizeof(response.status));
		ptr += sizeof(response.status);
		memcpy(ptr, &(response.nameLen), sizeof(response.nameLen));
		ptr += sizeof(response.nameLen);
		memcpy(ptr, (response.filename), response.nameLen);
		ptr += response.nameLen;
		memcpy(ptr, &(response.payload.m_size), sizeof(response.payload.m_size));
		ptr += sizeof(response.payload.m_size);
		memcpy(ptr, (response.payload.m_payload), size);
	}


	void destroy_ptr(uint8_t* ptr)
	{
		if (ptr != nullptr)
		{
			delete[] ptr;
			ptr = nullptr;
		}
	}


	void destroy(ServerResponse::Response* response)
	{
		if (response != nullptr)
		{
			destroy_ptr(response->filename);
			destroy_ptr(response->payload.m_payload);
			delete response;
			response = nullptr;
		}
	}

}