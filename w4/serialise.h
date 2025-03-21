#pragma once

#include <vector>
#include <string>
#include <deque>
#include <stdexcept>
#include <algorithm>

template <typename T>
uint64_t SizeOf (const T&) {
    return sizeof(T);
}

template <>
uint64_t SizeOf (const std::string& str) {
    return str.length();
}

class Writer {
    std::vector<uint8_t> buffer;
public:
    template <typename T>
    Writer& Write (const T& val) {
        uint64_t size = SizeOf(val);

        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&size), reinterpret_cast<uint8_t*>(&size + sizeof(size)));
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&val), reinterpret_cast<uint8_t*>(&val + size))

        return *this;
    }

    Writer& Writer::Write (const std::string& val) {
        uint64_t size = SizeOf(val);
    
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&size), reinterpret_cast<uint8_t*>(&size + sizeof(size)));
        buffer.insert(buffer.end(), val.c_str(), val.c_str() + size);
    
        return *this;
    }

    Writer& Writer::Write (const char* val) {
        uint64_t size = strlen(val);
    
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&size), reinterpret_cast<uint8_t*>(&size + sizeof(size)));
        buffer.insert(buffer.end(), val, val + size);
    
        return *this;
    }

    std::vector<uint8_t>&& Buffer () {
        auto res = std::vector<uint8_t>();
        std::swap(res, buffer);
        return std::move(res);
    }
private:

};

class Reader {
    std::deque<uint8_t> buffer;
public:
    Reader (const std::vector<uint8_t>& buffer) : buffer(buffer.begin(), buffer.end()) {}

    template <typename T>
    Reader& Read (T& val) {
        uint64_t size = 0;
        
        if (buffer.size() < sizeof(size)) throw std::runtime_error("not enought len to scan size");
        for (size_t i = 0; i < sizeof(size); i++) {
            reinterpret_cast<uint8_t*>(&size)[i] = buffer.front();
            buffer.pop_front();
        }

        // AHTUNG: изменить для сторки
        if (size != SizeOf(val))  throw std::runtime_error("mistmatch between type size and buffer len");
        if (buffer.size() < size) throw std::runtime_error("not enought len to scan type");

        for (size_t i = 0; i < size; i++) {
            reinterpret_cast<uint8_t*>(&val)[i] = buffer.front();
            buffer.pop_front();
        }

        return *this;
    }

    Reader& Read (std::string& val) {
        uint64_t size = 0;
        
        if (buffer.size() < sizeof(size)) throw std::runtime_error("not enought len to scan size");
        for (size_t i = 0; i < sizeof(size); i++) {
            reinterpret_cast<uint8_t*>(&size)[i] = buffer.front();
            buffer.pop_front();
        }

        if (buffer.size() < size) throw std::runtime_error("not enought len to scan type");
        
        val = std::string(buffer.front(), buffer.front() + size);
        for (size_t i = 0; i < size; i++) buffer.pop_front();

        return *this;
    }
};
