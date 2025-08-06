#include "stuff/Buffer.h"
#include "stuff/FileLoader.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <vector>
#include <map>
#include <cmath>

enum class Endianness {
    Big,
    Little
};

struct PDLObject {
    uint32_t type;
    float x, y, z;
    size_t offset;
};

std::string getTypeName(uint32_t type) {
    switch (type) {
        case 3437124069: return "Vehicle";
        case 1462988517: return "Road";
        default: return "Object";
    }
}

bool isReasonableCoord(float f) {
    return std::abs(f) < 100000.0f;
}

// --- Big Endian Readers ---
uint32_t readBEUInt32(const uint8_t* data) {
    return (uint32_t(data[0]) << 24) |
           (uint32_t(data[1]) << 16) |
           (uint32_t(data[2]) << 8)  |
           (uint32_t(data[3]));
}

float readBEFloat(const uint8_t* data) {
    uint32_t temp = readBEUInt32(data);
    float result;
    std::memcpy(&result, &temp, sizeof(result));
    return result;
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

std::vector<PDLObject> tryRecordSize(const Buffer& buffer, size_t recordSize, size_t& headerSizeOut, Endianness endian) {
    std::vector<PDLObject> result;
    size_t bestHeader = 0;

    for (size_t headerOffset = 0; headerOffset < 64; headerOffset += 4) {
        std::vector<PDLObject> objects;
        size_t offset = headerOffset;

        while (offset + recordSize <= buffer.size()) {
            const uint8_t* base = buffer.data() + offset;

            PDLObject obj;
            if (endian == Endianness::Big) {
                obj.type = readBEUInt32(base + 0);
                obj.x    = readBEFloat(base + 4);
                obj.y    = readBEFloat(base + 8);
                obj.z    = readBEFloat(base + 12);
            } else {
                obj.type = readLEUInt32(base + 0);
                obj.x    = readLEFloat(base + 4);
                obj.y    = readLEFloat(base + 8);
                obj.z    = readLEFloat(base + 12);
            }

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

        Buffer buffer = fileLoader::Load(inputFile);

        const std::vector<size_t> candidateRecordSizes = {16, 20, 24, 32};

        std::vector<PDLObject> bestObjects;
        size_t bestSize = 0;
        size_t bestHeader = 0;
        Endianness detectedEndian = Endianness::Big;

        for (auto size : candidateRecordSizes) {
            size_t headerBE, headerLE;

            auto objsBE = tryRecordSize(buffer, size, headerBE, Endianness::Big);
            auto objsLE = tryRecordSize(buffer, size, headerLE, Endianness::Little);

            if (objsBE.size() > bestObjects.size()) {
                bestObjects = objsBE;
                bestSize = size;
                bestHeader = headerBE;
                detectedEndian = Endianness::Big;
            }

            if (objsLE.size() > bestObjects.size()) {
                bestObjects = objsLE;
                bestSize = size;
                bestHeader = headerLE;
                detectedEndian = Endianness::Little;
            }
        }

        std::cout << "[cpdl] Detected record size: " << bestSize << " bytes\n";
        std::cout << "[cpdl] Skipped header bytes: " << bestHeader << "\n";
        std::cout << "[cpdl] Detected endianness: " << (detectedEndian == Endianness::Big ? "Big Endian" : "Little Endian") << "\n";
        std::cout << "[cpdl] Parsed " << bestObjects.size() << " objects.\n";

        // --- Write to unpacked file ---
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
