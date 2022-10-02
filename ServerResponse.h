#pragma once

#include <map>
#include <atomic>
#include <boost/asio/ip/tcp.hpp>
#include "ServerActions.h"


class ServerResponse
{
public:
    struct Response
    {
        enum EStatus
        {
            SUCCESS_RESTORE = 210,   // File was found and restored. all fields are valid.
            SUCCESS_DIR = 211,   // Files listing returned successfully. all fields are valid.
            SUCCESS_BACKUP_DELETE = 212,   // File was successfully backed up or deleted. size, payload are invalid. [From forum].
            ERROR_NOT_EXIST = 1001,  // File doesn't exist. size, payload are invalid.
            ERROR_NO_FILES = 1002,  // Client has no files. Only status & version are valid.
            ERROR_GENERIC = 1003   // Generic server error. Only status & version are valid.
        };

        const uint8_t version;    // Server Version
        uint16_t status;          // Request status
        uint16_t nameLen;         // FileName length
        uint8_t* filename;        // FileName
        Payload payload;
        Response() : version(SERVER_VERSION), status(0), nameLen(0), filename(nullptr) {}
        uint32_t sizeWithoutPayload() const { return (sizeof(version) + sizeof(status) + sizeof(nameLen) + nameLen + sizeof(payload.size)); }

    };
    
};

