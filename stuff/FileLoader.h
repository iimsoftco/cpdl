#pragma once
#include "Buffer.h"
#include <fstream>
#include <string>

namespace fileLoader {

    inline Buffer Load(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Failed to open file for reading");

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        Buffer buf;
        buf.resize(size);

        if (!file.read(reinterpret_cast<char*>(buf.data()), size))
            throw std::runtime_error("Failed to read file");

        return buf;
    }

    inline void Save(const std::string& filename, const Buffer& data) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) throw std::runtime_error("Failed to open file for writing");

        file.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    inline void AppendText(const std::string& filename, const std::string& text) {
        std::ofstream file(filename, std::ios::app);
        if (!file) throw std::runtime_error("Failed to open file for appending");

        file << text;
    }

}
