#pragma once
#include "raw_message.h"
#include "crc32.h"

#define MESSAGE_HEADER_SIZE sizeof(message_header::m_size) + sizeof(message_header::m_csum)

struct message_header {
	void read_from(uint8_t* buf) {
		m_size = *(decltype(m_size)*)(buf);
		m_csum = *(decltype(m_csum)*)(buf + sizeof(m_size));
	}

	void write_to(uint8_t* buf) {
		*(decltype(m_size)*)(buf) = m_size;
		*(decltype(m_csum)*)(buf + sizeof(m_size)) = m_csum;
	}

	uint32_t m_size;
	uint32_t m_csum;
};

struct tcp_datagram {
	tcp_datagram() {
		m_data = nullptr;
		m_header_ready = false;
	}

	~tcp_datagram() {
		if (m_data)
			delete m_data;
	}

	bool is_fully_processed() {
		return m_header_ready && m_data->m_pos == m_header.m_size;
	}

	// returns amount of read bytes from buf
	size_t read_data(uint8_t* buf, size_t len) {
		auto read_chunk = [&](size_t wanted_total_size) {
			size_t bytes_to_read = std::min(len, m_data->m_pos - wanted_total_size);
			memcpy(m_data->put_raw(bytes_to_read), buf, bytes_to_read);
			len -= bytes_to_read;
			buf += bytes_to_read;
		};

		// make sure this is allocated
		if (!m_data)
			m_data = new raw_message();

		size_t initial_len = len;
		// if the header isnt ready, we need to read that first.
		if (!m_header_ready) {
			// to get the header we need to read exactly MESSAGE_HEADER_SIZE bytes 
			read_chunk(MESSAGE_HEADER_SIZE);

			// when reaching the correct size, read the header and reset the data
			if (m_data->m_pos == MESSAGE_HEADER_SIZE) {
				m_header.read_from(m_data->m_buf);
				m_header_ready = true;
				m_data->m_pos = 0;
				m_data->m_size = 0;
			}
		}

		// need another check for this since this can be valid now
		if (m_header_ready) {
			read_chunk(m_header.m_size);
		}

		// calculate and return the amount of read bytes
		return initial_len - len;
	}

	void prepare_send(raw_message* data = nullptr) {
		if (data) {
			delete m_data;
			m_data = data;
		}

		m_data->m_pos = 0;
		m_header.m_size = m_data->m_size;
		m_header.m_csum = crc32(0, m_data->m_buf, m_data->m_size);
		m_header_ready = false; // this indicates that the header has not yet been written to the outbuf
	}

	// max_bytes is the maximum of bytes we want to write to buf
	// returns amount of bytes actually written
	size_t write_data(uint8_t* buf, size_t max_bytes) {
		size_t initial_max_bytes = max_bytes;

		if (!m_header_ready) {
			if (max_bytes < MESSAGE_HEADER_SIZE) {
				// failure, throw exception?
				return 0; 
			}

			m_header.write_to(buf);
			max_bytes -= MESSAGE_HEADER_SIZE;
			buf += MESSAGE_HEADER_SIZE;

			m_header_ready = true; // indicate header has been written for this datagram
		}

		size_t bytes_to_write = std::min(max_bytes, m_data->m_size - m_data->m_pos);
		memcpy(buf, m_data->get_raw(bytes_to_write), bytes_to_write);
		max_bytes -= bytes_to_write;

		return initial_max_bytes - max_bytes;
	}

	message_header m_header;
	raw_message* m_data;

private:
	bool m_header_ready;
};