#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MHXX {
namespace Loader {

struct RomInfo {
    bool is_valid = false;
    std::string title_id;
    std::string product_code;
    uint64_t rom_size = 0;
    uint64_t part0_offset = 0;
    uint64_t exefs_offset = 0;
    uint64_t exefs_size = 0;
    uint64_t romfs_offset = 0;
    uint64_t romfs_size = 0;
    uint32_t entry_point = 0x00100000;
};

// BLZ Decompression (Backward LZSS) for 3DS ExeFS .code
bool DecompressBLZ(const uint8_t* compressed_data, size_t compressed_size, std::vector<uint8_t>& out_decompressed);

// Load ROM and extract ExeFS code
bool LoadRomAndExtractCode(const std::string& romfs_dir, RomInfo& info, std::vector<uint8_t>& out_code);

} // namespace Loader
} // namespace MHXX

