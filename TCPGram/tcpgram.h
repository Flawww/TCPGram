#pragma once
#include <deque>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include "datagram.h"

using boost::asio::ip::tcp;
typedef bool(*proccess_callback_t)(tcp::socket*, raw_message*);

#define MAX_BUFLEN 2048
#define MAX_LENGTH_PER_SEND 1324

/**
* @brief reads/sends tcp stream as datagram
*/
class tcpgram {
public:
	/**
	* @brief Constructs the tcgram object
	*
	* @param socket the socket we want to use for this instance (used for sending)
	* @param process_dgram_fn functions that gets called with arguments (socket, buffer, buffer size) when a datagram is fully recieved
	*/
	tcpgram(tcp::socket* socket, proccess_callback_t process_dgram_fn, size_t max_datagram_size = MAXSIZE_T) {
		m_max_datagram_size = max_datagram_size;
		m_process_dgram_fn = process_dgram_fn;
		m_datagram_reader = nullptr;
		m_socket = socket;
	}

	~tcpgram() {
		if (m_datagram_reader)
			delete m_datagram_reader;
	}

	/**
	* @brief Recieves data on the socket and calls process_dgram_fn when a datagram is fully recieved
	* 
	* NOTE: This function will only perform ONE read on the socket, preventing a potential malicious client 
	* from continously sending data to make the recieving thread get stuck.
	* 
	* @return false if the read fails, that usually means its time to get rid of the socket in question.
	*/
	bool recieve() {
		boost::array<uint8_t, MAX_BUFLEN> inbuf;
		boost::system::error_code error;

		size_t len = m_socket->read_some(boost::asio::buffer(inbuf), error);
		
		if (error) {
			if (error == boost::asio::error::try_again || error == boost::asio::error::would_block)
				return true; // no failure
			// should we also throw the error so caller can handle it???
			return false;
		}
		
		// loop until we read the recieved buf
		while (len) {
			if (!m_datagram_reader) {
				m_datagram_reader = new tcp_datagram();
			}

			size_t read = m_datagram_reader->read_data((uint8_t*)inbuf.data(), len);
			len -= read;

			// make sure the datagram we are recieving isnt too big 
			if (m_datagram_reader->m_header_ready && m_datagram_reader->m_header.m_size > m_max_datagram_size) {
				return false; 
			}

			// when we fully recieved this datagram, execute our callback and then clean up
			if (m_datagram_reader->is_fully_processed()) {

				// ... but first lets make sure the crc is correct
				if (m_datagram_reader->m_header.m_csum != crc32(0, m_datagram_reader->m_data->m_buf, m_datagram_reader->m_data->m_size)) {
					return false;
				}

				m_datagram_reader->m_data->m_pos = 0; // reset the pos
				if (!m_process_dgram_fn(m_socket, m_datagram_reader->m_data)) {
					return false;
				}

				// clean up
				delete m_datagram_reader;
				m_datagram_reader = nullptr;
			}
		}

		return true;
	}

	/**
	* @see send(raw_message* datagram) 
	* 
	* @param buf buffer that holds data to send. Cleaned up by caller
	* @param len length of data in buf
	* @return false if fatal error occurs, time to close the socket.
	*/
	bool send(uint8_t* buf, size_t len) {
		raw_message* msg = new raw_message(buf, len);
		return send(msg);
	}

	/**
	* @brief Sends a datagram to the tcp socket
	* 
	* @param datagram raw_message containing the datagram we want to send. Cleaned up by caller
	* @return false if fatal error occurs, time to close the socket.
	*/
	bool send(raw_message* datagram) {
		uint8_t outbuffer[MAX_BUFLEN];
		tcp_datagram datagram_writer;
		boost::system::error_code error;

		datagram_writer.prepare_send(datagram);

		// loop until whole datagram has been sent
		while (!datagram_writer.is_fully_processed()) {
			size_t written = datagram_writer.write_data(outbuffer, MAX_LENGTH_PER_SEND);

			// send until we sent everything
			size_t res = 0;
			while (res < written) {
				size_t len = m_socket->write_some(boost::asio::buffer(outbuffer + res, written - res), error);

				if (error) {
					if (error == boost::asio::error::try_again || error == boost::asio::error::would_block)
						continue; // try again

					datagram_writer.m_data = nullptr;
					return false; // fail
				}

				res += len;
			}
		}

		datagram_writer.m_data = nullptr;
		return true;
	}

private:

	/**
	* Callback function to process the recieved datagram
	*/
	proccess_callback_t m_process_dgram_fn;

	/**
	* The datagram currently being recieved. Free'd on full recieve
	*/
	tcp_datagram* m_datagram_reader;

	/**
	* Max allowed size of recieved datagram. If a datagram exceeds this size the read() will promptly fail.
	*/
	size_t m_max_datagram_size;

	/**
	* Socket that we want to read and write to
	*/
	tcp::socket* m_socket;
};