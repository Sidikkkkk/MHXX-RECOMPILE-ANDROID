#include "recompiler/rom_loader.h"
#include "bridge/logger.h"
#include <cstdio>
#include <cstring>
#include <vector>

namespace MHXX {
namespace Loader {

// 3DS BLZ Backward LZSS Decompressor
bool DecompressBLZ(const uint8_t* compressed_data, size_t compressed_size, std::vector<uint8_t>& out_decompressed) {
    if (compressed_size < 12) {
        SendLogToUI("[BLZ] Error: Ukuran data terlalu kecil (%zu bytes)", compressed_size);
        return false;
    }

    size_t file_len = compressed_size;
    size_t trailer_offset = file_len - 8;

    uint32_t enc_raw = *reinterpret_cast<const uint32_t*>(&compressed_data[trailer_offset]);
    uint32_t enc_len = enc_raw & 0x00FFFFFF;
    uint8_t header_len = compressed_data[trailer_offset + 3];
    uint32_t extra_size = *reinterpret_cast<const uint32_t*>(&compressed_data[trailer_offset + 4]);

    size_t total_decomp_size = file_len + extra_size;
    size_t uncompressed_prefix = file_len - header_len - enc_len;

    SendLogToUI("[BLZ] Dekompresi biner: Comp=%zu MB -> Decomp=%zu MB", 
                compressed_size / (1024 * 1024), total_decomp_size / (1024 * 1024));

    out_decompressed.resize(total_decomp_size);
    std::memcpy(out_decompressed.data(), compressed_data, uncompressed_prefix);

    int64_t read_ptr = static_cast<int64_t>(file_len - header_len - 1);
    int64_t write_ptr = static_cast<int64_t>(total_decomp_size - 1);
    int64_t prefix_limit = static_cast<int64_t>(uncompressed_prefix);

    while (write_ptr >= prefix_limit) {
        if (read_ptr < 0) break;
        uint8_t flag = compressed_data[read_ptr--];

        for (int bit = 7; bit >= 0; --bit) {
            if (write_ptr < prefix_limit) break;

            if ((flag & (1 << bit)) == 0) {
                // Byte literal
                out_decompressed[write_ptr--] = compressed_data[read_ptr--];
            } else {
                // Rujukan berulang (LZSS Back-reference)
                if (read_ptr < 1) break;
                uint8_t b1 = compressed_data[read_ptr--];
                uint8_t b2 = compressed_data[read_ptr--];
                int length = (b1 >> 4) + 3;
                int disp = (((b1 & 0x0F) << 8) | b2) + 3;

                for (int i = 0; i < length; ++i) {
                    if (write_ptr + disp < static_cast<int64_t>(total_decomp_size)) {
                        out_decompressed[write_ptr] = out_decompressed[write_ptr + disp];
                    }
                    write_ptr--;
                }
            }
        }
    }

    SendLogToUI("[BLZ] Sukses: Dekompresi selesai (%zu bytes)", out_decompressed.size());
    return true;
}

bool LoadRomAndExtractCode(const std::string& romfs_dir, RomInfo& info, std::vector<uint8_t>& out_code) {
    // 1. Cek apakah code_decompressed.bin sudah dicache sebelumnya
    std::string decomp_cache = romfs_dir + "/code_decompressed.bin";
    FILE* f_cache = fopen(decomp_cache.c_str(), "rb");
    if (f_cache) {
        fseek(f_cache, 0, SEEK_END);
        long sz = ftell(f_cache);
        fseek(f_cache, 0, SEEK_SET);
        if (sz > 10 * 1024 * 1024) { // Sekitar 12.4 MB
            SendLogToUI("[LOADER] Ditemukan cache biner: code_decompressed.bin (%.2f MB)", sz / (1024.0 * 1024.0));
            out_code.resize(sz);
            fread(out_code.data(), 1, sz, f_cache);
            fclose(f_cache);
            info.is_valid = true;
            info.entry_point = 0x00100000;
            return true;
        }
        fclose(f_cache);
    }

    // 2. Buka game.bin
    std::string game_bin = romfs_dir + "/game.bin";
    FILE* f = fopen(game_bin.c_str(), "rb");
    if (!f) {
        SendLogToUI("[LOADER] Gagal membuka: %s", game_bin.c_str());
        return false;
    }

    fseek(f, 0, SEEK_END);
    info.rom_size = ftell(f);
    SendLogToUI("[LOADER] Membaca ROM Cartridge (%.2f GB)...", info.rom_size / (1024.0 * 1024.0 * 1024.0));

    // Periksa NCSD Magic pada offset 0x100
    fseek(f, 0x100, SEEK_SET);
    char ncsd_magic[5] = {0};
    fread(ncsd_magic, 1, 4, f);
    if (std::memcmp(ncsd_magic, "NCSD", 4) != 0) {
        SendLogToUI("[LOADER] Peringatan: Header NCSD tidak terdeteksi (Magic: %s)", ncsd_magic);
    }

    // Baca Partition 0 Media Units pada offset 0x120
    fseek(f, 0x120, SEEK_SET);
    uint32_t part0_mu = 0;
    fread(&part0_mu, 4, 1, f);
    info.part0_offset = static_cast<uint64_t>(part0_mu) * 0x200;
    SendLogToUI("[LOADER] Partisi Game NCCH 0 pada offset: 0x%lX", (unsigned long)info.part0_offset);

    // Periksa NCCH Header
    fseek(f, info.part0_offset + 0x100, SEEK_SET);
    char ncch_magic[5] = {0};
    fread(ncch_magic, 1, 4, f);

    // Product Code pada offset 0x150
    fseek(f, info.part0_offset + 0x150, SEEK_SET);
    char prod_code[17] = {0};
    fread(prod_code, 1, 16, f);
    info.product_code = prod_code;
    SendLogToUI("[LOADER] NCCH Product Code: %s", prod_code);

    // ExeFS Offset & Size pada offset 0x1A0
    fseek(f, info.part0_offset + 0x1A0, SEEK_SET);
    uint32_t exefs_mu = 0;
    uint32_t exefs_sz_mu = 0;
    fread(&exefs_mu, 4, 1, f);
    fread(&exefs_sz_mu, 4, 1, f);
    info.exefs_offset = info.part0_offset + static_cast<uint64_t>(exefs_mu) * 0x200;
    info.exefs_size = static_cast<uint64_t>(exefs_sz_mu) * 0x200;

    // RomFS Offset & Size pada offset 0x1B0
    fseek(f, info.part0_offset + 0x1B0, SEEK_SET);
    uint32_t romfs_mu = 0;
    uint32_t romfs_sz_mu = 0;
    fread(&romfs_mu, 4, 1, f);
    fread(&romfs_sz_mu, 4, 1, f);
    info.romfs_offset = info.part0_offset + static_cast<uint64_t>(romfs_mu) * 0x200;
    info.romfs_size = static_cast<uint64_t>(romfs_sz_mu) * 0x200;
    SendLogToUI("[LOADER] ExeFS: 0x%lX (%.2f MB), RomFS: 0x%lX (%.2f MB)", 
                (unsigned long)info.exefs_offset, info.exefs_size / (1024.0 * 1024.0),
                (unsigned long)info.romfs_offset, info.romfs_size / (1024.0 * 1024.0));

    // Baca ExeFS Header (512 bytes / 10 file entries)
    fseek(f, info.exefs_offset, SEEK_SET);
    uint8_t exefs_hdr[512];
    fread(exefs_hdr, 1, 512, f);

    uint32_t code_offset = 0;
    uint32_t code_size = 0;
    bool found_code = false;

    for (int i = 0; i < 10; ++i) {
        char name[9] = {0};
        std::memcpy(name, &exefs_hdr[i * 16], 8);
        uint32_t offset = *reinterpret_cast<uint32_t*>(&exefs_hdr[i * 16 + 8]);
        uint32_t size = *reinterpret_cast<uint32_t*>(&exefs_hdr[i * 16 + 12]);

        if (name[0] != 0) {
            SendLogToUI("[LOADER] ExeFS Entry: %s (Offset: 0x%X, Size: %u bytes)", name, offset, size);
            if (std::strcmp(name, ".code") == 0) {
                code_offset = offset;
                code_size = size;
                found_code = true;
            }
        }
    }

    if (!found_code) {
        SendLogToUI("[LOADER] Error: .code tidak ditemukan di ExeFS!");
        fclose(f);
        return false;
    }

    // Baca data .code (terkompresi BLZ)
    uint64_t code_file_pos = info.exefs_offset + 512 + code_offset;
    fseek(f, code_file_pos, SEEK_SET);
    std::vector<uint8_t> compressed_code(code_size);
    fread(compressed_code.data(), 1, code_size, f);
    fclose(f);

    SendLogToUI("[LOADER] Membaca .code (%u bytes) dari ExeFS...", code_size);

    // Dekompresi BLZ
    if (!DecompressBLZ(compressed_code.data(), compressed_code.size(), out_code)) {
        SendLogToUI("[LOADER] Dekompresi gagal, menggunakan biner mentah.");
        out_code = std::move(compressed_code);
    }

    // Simpan cache biner agar boot selanjutnya instan
    FILE* f_save = fopen(decomp_cache.c_str(), "wb");
    if (f_save) {
        fwrite(out_code.data(), 1, out_code.size(), f_save);
        fclose(f_save);
        SendLogToUI("[LOADER] Cache biner disimpan ke: %s", decomp_cache.c_str());
    }

    info.is_valid = true;
    info.entry_point = 0x00100000;
    return true;
}

} // namespace Loader
} // namespace MHXX
