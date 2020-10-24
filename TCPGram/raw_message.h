#pragma once
#include <cstdint>
#include <string>

class raw_message {
public:
    raw_message() {
        m_buf = new uint8_t[0x1000];
        m_buf_size = 0x1000;
        m_pos = 0;
        m_size = 0;
    }

    // init from passed buffer
    raw_message(uint8_t* buf, size_t size) {
        if (size & 0xFFF)
            m_buf_size = (size + 0x1000) & ~(0xFFF);
        else
            m_buf_size = size;

        m_buf = new uint8_t[m_buf_size];
        memcpy(m_buf, buf, size);
        m_size = size;
    }

    // pre-allocate
    raw_message(size_t size) {
        if (size & 0xFFF)
            m_buf_size = (size + 0x1000) & ~(0xFFF);
        else
            m_buf_size = size;

        m_buf = new uint8_t[m_buf_size];
    }

    ~raw_message() {
        delete[] m_buf;
    }

    void expand_buffer() {
        auto tmp = new uint8_t[m_buf_size + 0x1000];
        memcpy(tmp, m_buf, m_size);
        m_buf_size += 0x1000;
        delete[] m_buf;
        m_buf = tmp;
    }


    // Put functions - writing to the buffer
    template <typename t>
    void put(t value) {
        write(&value, sizeof(t));
    }

    void put(char* value) {
        write((const char*)value, strlen(value) + 1);
    }

    void put(const char* value) {
        write(value, strlen(value) + 1);
    }

    void* put_raw(size_t size) {
        size_t min_size = m_pos + size;
        while (m_buf_size < min_size)
            expand_buffer();

        void* retp = m_buf + m_pos;
        m_pos += size;
        m_size += size;
        return retp;
    }

    // Get functions - reading from the buffer
    template <typename t>
    bool get(t& value) {
        return read(&value, sizeof(t));
    }

    bool get(char*& value) {
        size_t last_pos = m_pos;
        while (last_pos < m_buf_size && m_buf[last_pos])
            last_pos++;

        if (last_pos >= m_buf_size)
            return false;

        value = (char*)(m_buf + m_pos);
        m_pos = last_pos + 1;
        return true;
    }

    void* get_raw(size_t size) {
        size_t min_size = m_pos + size;
        if (m_buf_size < min_size)
            return nullptr;
        void* retp = m_buf + m_pos;
        m_pos += size;
        return retp;
    }


    uint8_t* m_buf;
    size_t m_buf_size;
    size_t m_size; // size of the actual written data to this buffer
    size_t m_pos; // used when reading 

private:

    void write(const void* buf, size_t size) {
        size_t min_size = m_pos + size;
        while (m_buf_size < min_size) // resize the buffer
            expand_buffer();

        memcpy(m_buf + m_pos, buf, size);
        m_size += size;
        m_pos += size;
    }

    bool read(void* buf, size_t size) {
        size_t min_size = m_pos + size;
        if (m_size < min_size)
            return false;

        memcpy(buf, m_buf + m_pos, size);
        m_pos += size;
        return true;
    }
};