#include "stuff/Buffer.h"
#include "stuff/FileLoader.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cmath>
#include <openssl/aes.h>
#include <openssl/evp.h>

struct PDLObject {
    uint32_t type;
    float x, y, z;
    size_t offset;
};

std::string getTypeName(uint32_t type) {
    switch (type) {
        case 3274399645: return "Vehicle";
        default: return "Object";
    }
}

bool isReasonableCoord(float f) {
    return std::abs(f) < 100000.0f;
}

// --- Little Endian Readers ---
uint32_t readLEUInt32(const uint8_t* data) {
    return (uint32_t(data[3]) << 24) |
           (uint32_t(data[2]) << 16) |
           (uint32_t(data[1]) << 8)  |
           (uint32_t(data[0]));
}

float readLEFloat(const uint8_t* data) {
    uint32_t temp = readLEUInt32(data);
    float result;
    std::memcpy(&result, &temp, sizeof(result));
    return result;
}

// --- AES-128 ECB Decryption ---
Buffer decryptAES128ECB(const Buffer& encrypted, const std::string& keyString) {
    if (keyString.size() > 16)
        throw std::runtime_error("AES key too long (must be 16 bytes for AES-128)");

    uint8_t key[16] = {0};
    std::memcpy(key, keyString.c_str(), std::min<size_t>(16, keyString.size()));

    Buffer decrypted;
    decrypted.resize(encrypted.size());

    AES_KEY aesKey;
    if (AES_set_decrypt_key(key, 128, &aesKey) < 0) {
        throw std::runtime_error("Failed to set AES decryption key");
    }

    for (size_t i = 0; i + 16 <= encrypted.size(); i += 16) {
        AES_ecb_encrypt(encrypted.data() + i, decrypted.data() + i, &aesKey, AES_DECRYPT);
    }

    return decrypted;
}

// --- Record Size Guesser (Little Endian only) ---
std::vector<PDLObject> tryRecordSize(const Buffer& buffer, size_t recordSize, size_t& headerSizeOut) {
    std::vector<PDLObject> result;
    size_t bestHeader = 0;

    for (size_t headerOffset = 0; headerOffset < 64; headerOffset += 4) {
        std::vector<PDLObject> objects;
        size_t offset = headerOffset;

        while (offset + recordSize <= buffer.size()) {
            const uint8_t* base = buffer.data() + offset;

            PDLObject obj;
            obj.type = readLEUInt32(base + 0);
            obj.x    = readLEFloat(base + 4);
            obj.y    = readLEFloat(base + 8);
            obj.z    = readLEFloat(base + 12);
            obj.offset = offset;

            if (isReasonableCoord(obj.x) && isReasonableCoord(obj.y) && isReasonableCoord(obj.z)) {
                objects.push_back(obj);
            } else {
                break;
            }

            offset += recordSize;
        }

        if (objects.size() > result.size()) {
            result = objects;
            bestHeader = headerOffset;
        }
    }

    headerSizeOut = bestHeader;
    return result;
}

int main() {
    try {
        std::string inputFile = "map.pdl";
        std::string outputFile = "map_unpacked.txt";
        std::string aesKey = "Planet Droidia";  // 15 bytes, will be padded

        // Load and decrypt
        Buffer encryptedBuffer = fileLoader::Load(inputFile);
        

        Buffer buffer = decryptAES128ECB(encryptedBuffer, aesKey);

        const std::vector<size_t> candidateRecordSizes = {16, 20, 24, 32};
        std::vector<PDLObject> bestObjects;
        size_t bestSize = 0;
        size_t bestHeader = 0;

        for (auto size : candidateRecordSizes) {
            size_t headerLE;
            auto objsLE = tryRecordSize(buffer, size, headerLE);

            if (objsLE.size() > bestObjects.size()) {
                bestObjects = objsLE;
                bestSize = size;
                bestHeader = headerLE;
            }
        }

        std::cout << "[cpdl] Detected record size: " << bestSize << " bytes\n";
        std::cout << "[cpdl] Skipped header bytes: " << bestHeader << "\n";
        std::cout << "[cpdl] Parsed " << bestObjects.size() << " objects (Little Endian only).\n";

        std::ofstream out(outputFile);
        if (!out.is_open()) {
            std::cerr << "[cpdl] Error: Failed to open output file.\n";
            return 1;
        }

        out << "# type_id type_name x y z\n";
        for (const auto& o : bestObjects) {
            out << o.type << " " << getTypeName(o.type) << " "
                << std::fixed << std::setprecision(6)
                << o.x << " " << o.y << " " << o.z << "\n";
        }

        out.close();
        std::cout << "[cpdl] Unpacked file written to: " << outputFile << "\n";

    } catch (const std::exception& e) {
        std::cerr << "[cpdl] Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
