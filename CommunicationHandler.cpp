#include "CommunicationHandler.h"
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include "ServerRequest.h"
#include "ServerResponse.h"
#include <boost/asio.hpp>
using boost::asio::ip::tcp;
#define PACKET_SIZE  1024


namespace CommunicationHandler {

	bool receive(boost::asio::ip::tcp::socket& sock, uint8_t* (buffer))
	{
		try
		{
			memset(buffer, 0, PACKET_SIZE);  // reset array before copying.
			sock.non_blocking(false);             // make sure socket is blocking.
			(void)boost::asio::read(sock, boost::asio::buffer(buffer, PACKET_SIZE));
			return true;
		}
		catch (boost::system::system_error&)
		{
			return false;
		}
	}

	bool handleSocketFromThread(boost::asio::ip::tcp::socket& sock, std::stringstream& err)
	{
		try
		{
			uint8_t buffer[PACKET_SIZE];
			Request* request = nullptr;    // allocated in deserializeRequest()
			ServerResponse::Response* response = nullptr;  // allocated in handleRequest()
			bool responseSent = false;      // response was sent ?

			if (!(receive(sock, buffer)))
			{
				err << "CServerLogic::handleSocketFromThread: Failed to receive first message from socket!" << std::endl;
				return false;
			}
			request = ServerRequestFuncs::deserializeRequest(buffer, PACKET_SIZE);
			while (ServerRequestFuncs::lock(*request) == false)  // If server is handling already exact user's ID request
			{
				std::this_thread::sleep_for(std::chrono::seconds(3));
			}
			const bool success = ServerRequestFuncs::handleRequest(*request, response, responseSent, sock, err);

			// Free allocated memory.
			if (!responseSent)
			{
				serializeResponse(*response, buffer);
				if (!CommunicationHandler::send(sock, buffer))
				{
					err << "Response sending on socket failed!" << std::endl;
					destroy(response);
					return false;
				}
				destroy(response);
				sock.close();
			}

			unlock(*request);  // release lock on user id
			destroy(request);

			return success;
		}
		catch (std::exception& e)
		{
			err << "Exception in thread: " << e.what() << "\n";
			return false;
		}
	}


	/**
	@brief receive(blocking) PACKET_SIZE bytes from socket.
	@param sock the socket to receive from.
	@param buffer an array of size PACKET_SIZE.The data will be copied to the array.
	@return number of bytes actually received.
	**/



	/**
	   @brief send (blocking) PACKET_SIZE bytes to socket.
	   @param sock the socket to send to.
	   @param buffer an array of size PACKET_SIZE. The data to send will be read from the array.
	   @return true if successfuly sent. false otherwise.
	 */
	bool send(boost::asio::ip::tcp::socket& sock, uint8_t* (buffer)[PACKET_SIZE])
	{
		try
		{
			sock.non_blocking(false);  // make sure socket is blocking.
			boost::system::error_code error;
			(void)boost::asio::write(sock, boost::asio::buffer(buffer, PACKET_SIZE), error);
			return true;
		}
		catch (boost::system::system_error&)
		{
			return false;
		}
	}

}