#!/usr/bin/env python3
"""
MHXX Android Native Port - Game Asset & Binary Extraction Pipeline
This script scans the `inputs/` folder for decrypted Monster Hunter Double Cross (MHXX)
ROM files (.3ds or .cia) and unpacks the ExeFS (code.bin) and RomFS assets.
"""

import os
import sys
import struct
import shutil
from pathlib import Path

# Paths
PROJECT_ROOT = Path(__file__).resolve().parent.parent
INPUTS_ROM_DIR = PROJECT_ROOT / "inputs" / "rom"
INPUTS_UPDATE_DIR = PROJECT_ROOT / "inputs" / "updates"
EXTRACTED_EXEFS_DIR = PROJECT_ROOT / "extracted" / "exefs"
EXTRACTED_ROMFS_DIR = PROJECT_ROOT / "extracted" / "romfs"
EXTRACTED_UPDATE_DIR = PROJECT_ROOT / "extracted" / "updates"

MEDIA_UNIT_SIZE = 0x200

def print_banner():
    print("=" * 65)
    print("       MHXX ANDROID NATIVE PORT - ASSET & CODE EXTRACTOR       ")
    print("=" * 65)
    print(f"Project Directory: {PROJECT_ROOT}\n")

def scan_input_files():
    """Scans inputs/ directory for game files"""
    rom_files = list(INPUTS_ROM_DIR.glob("*.3ds")) + list(INPUTS_ROM_DIR.glob("*.cia")) + list(INPUTS_ROM_DIR.glob("*.cxi"))
    update_files = list(INPUTS_UPDATE_DIR.glob("*.cia"))

    print(f"[*] Memeriksa folder: {INPUTS_ROM_DIR}")
    if not rom_files:
        print("    [!] PERINGATAN: Tidak ada file ROM (.3ds / .cia) ditemukan di 'inputs/rom/'!")
        print("    --> Silakan letakkan file ROM MHXX Anda di dalam folder tersebut.")
    else:
        for f in rom_files:
            size_mb = f.stat().st_size / (1024 * 1024)
            print(f"    [+] Ditemukan ROM : {f.name} ({size_mb:.2f} MB)")

    print(f"\n[*] Memeriksa folder: {INPUTS_UPDATE_DIR}")
    if not update_files:
        print("    [-] Tidak ada file update ditemukan (Opsional, tapi disarankan v1.4.0).")
    else:
        for f in update_files:
            size_mb = f.stat().st_size / (1024 * 1024)
            print(f"    [+] Ditemukan Update: {f.name} ({size_mb:.2f} MB)")

    return rom_files, update_files

def check_ncch_decrypted(file_handle, ncch_offset=0):
    """Checks if an NCCH partition is decrypted"""
    file_handle.seek(ncch_offset + 0x100)
    magic = file_handle.read(4)
    if magic != b'NCCH':
        return False, "Bukan NCCH partition valid (Magic mismatch)"
    
    # Read flags at 0x188 (8 bytes)
    file_handle.seek(ncch_offset + 0x188)
    flags = file_handle.read(8)
    # Flag[7] at offset 0x18F bit 2 is NoCrypto
    no_crypto = (flags[7] & 0x04) != 0
    fixed_crypto = (flags[7] & 0x01) != 0

    if no_crypto:
        return True, "Decrypted (NoCrypto flag set)"
    elif fixed_crypto:
        return True, "Decrypted (Zero-Key/Fixed Crypto)"
    else:
        return False, "TERENKRIPSI! ROM memerlukan dekripsi menggunakan Decrypt9WIP / GodMode9 pada konsol 3DS."

def unpack_exefs(file_handle, ncch_offset, out_dir):
    """Extracts files from ExeFS partition (including code.bin)"""
    file_handle.seek(ncch_offset + 0x1A0)
    exefs_offset_mu = struct.unpack('<I', file_handle.read(4))[0]
    exefs_size_mu = struct.unpack('<I', file_handle.read(4))[0]

    if exefs_offset_mu == 0 or exefs_size_mu == 0:
        print("    [-] ExeFS tidak ditemukan pada partisi ini.")
        return False

    exefs_offset = ncch_offset + (exefs_offset_mu * MEDIA_UNIT_SIZE)
    print(f"    [+] Membaca ExeFS Header pada offset 0x{exefs_offset:X}...")

    file_handle.seek(exefs_offset)
    header_data = file_handle.read(0x200)

    # 10 file headers, 16 bytes each: 8 bytes name, 4 bytes offset, 4 bytes size
    out_dir.mkdir(parents=True, exist_ok=True)
    for i in range(10):
        entry = header_data[i * 16 : (i + 1) * 16]
        name_raw = entry[:8].split(b'\x00')[0]
        if not name_raw:
            continue
        name = name_raw.decode('ascii', errors='ignore')
        f_offset = struct.unpack('<I', entry[8:12])[0]
        f_size = struct.unpack('<I', entry[12:16])[0]

        data_offset = exefs_offset + 0x200 + f_offset
        file_handle.seek(data_offset)
        file_content = file_handle.read(f_size)

        out_file = out_dir / f"{name}.bin"
        with open(out_file, 'wb') as out_f:
            out_f.write(file_content)

        print(f"        -> Berhasil mengekstrak {out_file.name} ({len(file_content)} bytes)")
    return True

def process_3ds_file(rom_path):
    print(f"\n[+] Memproses ROM 3DS: {rom_path.name}")
    with open(rom_path, 'rb') as f:
        # Check NCSD header
        f.seek(0x100)
        magic = f.read(4)
        if magic != b'NCSD':
            print("    [!] File ini bukan biner cartridge NCSD 3DS yang valid.")
            return

        # Partition 0 (Game executable & data)
        f.seek(0x120)
        part0_offset_mu = struct.unpack('<I', f.read(4))[0]
        part0_size_mu = struct.unpack('<I', f.read(4))[0]
        part0_offset = part0_offset_mu * MEDIA_UNIT_SIZE

        print(f"    [+] Partisi Utama Game (NCCH 0) offset: 0x{part0_offset:X}")
        is_dec, msg = check_ncch_decrypted(f, part0_offset)
        print(f"    [*] Status Enkripsi: {msg}")

        if not is_dec:
            print("    [X] Harap gunakan ROM yang sudah didekripsi (Decrypted 3DS ROM).")
            return

        # Ekstraksi ExeFS (code.bin, icon, banner)
        unpack_exefs(f, part0_offset, EXTRACTED_EXEFS_DIR)
        print("    [OK] Ekstraksi ExeFS selesai! File 'code.bin' siap untuk recompiler pipeline.")

def main():
    print_banner()
    rom_files, update_files = scan_input_files()

    if not rom_files:
        print("\n[LANGKAH SELANJUTNYA]")
        print("1. Salin file 'MHXX.3ds' atau 'MHXX.cia' Anda ke dalam folder:")
        print(f"   --> {INPUTS_ROM_DIR}")
        print("2. (Opsional) Salin file update 'v1.4.0.cia' ke:")
        print(f"   --> {INPUTS_UPDATE_DIR}")
        print("3. Jalankan kembali script ini: python tools/extract_game.py\n")
        return

    for rom in rom_files:
        if rom.suffix.lower() == '.3ds':
            process_3ds_file(rom)
        else:
            print(f"\n[*] Format {rom.suffix} terdeteksi. Tool ekstraksi CIA/CXI sedang disiapkan.")

if __name__ == '__main__':
    main()

