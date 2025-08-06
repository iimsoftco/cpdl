#include "stuff/Buffer.h"
#include "stuff/FileLoader.h"

#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <vector>
#include <map>
#include <cmath>

// Struct for dynamic object representation
struct PDLObject {
    uint32_t type;
    float x, y, z;
    size_t offset;
};

// Return known type names (you can adjust based on real findings)
std::string getTypeName(uint32_t type) {
    switch (type) {
        case 3437124069: return "Vehicle";
        case 1462988517: return "Road";
        default: return "Object";
    }
}

// Helper to check if float is within reasonable range
bool isReasonableCoord(float f) {
    return std::abs(f) < 100000.0f;  // tweak this limit if needed
}

// Attempt parsing with specific record size
std::vector<PDLObject> tryRecordSize(const Buffer& buffer, size_t recordSize, size_t& headerSizeOut) {
    std::vector<PDLObject> result;
    size_t bestHeader = 0;

    for (size_t headerOffset = 0; headerOffset < 64; headerOffset += 4) {  // try various header skips
        std::vector<PDLObject> objects;
        size_t offset = headerOffset;

        while (offset + recordSize <= buffer.size()) {
            PDLObject obj;
            std::memcpy(&obj.type, buffer.data() + offset + 0, 4);
            std::memcpy(&obj.x,    buffer.data() + offset + 4, 4);
            std::memcpy(&obj.y,    buffer.data() + offset + 8, 4);
            std::memcpy(&obj.z,    buffer.data() + offset + 12, 4);
            obj.offset = offset;

            if (isReasonableCoord(obj.x) && isReasonableCoord(obj.y) && isReasonableCoord(obj.z)) {
                objects.push_back(obj);
            } else {
                break;  // Unreasonable entry
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
        Buffer buffer = fileLoader::Load("map.pdl");

        const std::vector<size_t> candidateRecordSizes = {16, 20, 24, 32};
        std::vector<PDLObject> bestObjects;
        size_t bestSize = 0;
        size_t bestHeader = 0;

        for (auto size : candidateRecordSizes) {
            size_t headerSize;
            auto objects = tryRecordSize(buffer, size, headerSize);

            if (objects.size() > bestObjects.size()) {
                bestObjects = objects;
                bestSize = size;
                bestHeader = headerSize;
            }
        }

        std::cout << "[cpdl] Detected record size: " << bestSize << " bytes\n";
        std::cout << "[cpdl] Skipped header bytes: " << bestHeader << "\n";
        std::cout << "[cpdl] Parsed " << bestObjects.size() << " objects:\n\n";

        std::map<uint32_t, int> typeCounts;
        for (size_t i = 0; i < bestObjects.size(); ++i) {
            const auto& o = bestObjects[i];
            std::string name = getTypeName(o.type);
            typeCounts[o.type]++;

            std::cout << std::setw(3) << i << ". Offset: 0x"
                      << std::hex << std::setw(6) << o.offset << std::dec
                      << " | Type ID: " << o.type
                      << " (" << name << ")"
                      << " | Pos: (" << std::fixed << std::setprecision(2)
                      << o.x << ", " << o.y << ", " << o.z << ")\n";
        }

        std::cout << "\n[cpdl] Type Frequencies:\n";
        for (const auto& [type, count] : typeCounts) {
            std::cout << "  Type " << type
                      << " (" << getTypeName(type) << "): "
                      << count << " objects\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "[cpdl] Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
