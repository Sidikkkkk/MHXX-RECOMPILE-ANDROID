#pragma once

#include <cstdint>
#include <string>

namespace MHXX {
namespace CTR {
namespace IPC {

// Thread Local Storage (TLS) IPC Command Buffer
// Pada 3DS, TLS IPC buffer terletak pada offset 0x80 dari TLS pointer (0x1FF80080)
struct CommandHeader {
    uint16_t param_words;
    uint16_t command_id;
};

inline CommandHeader ParseHeader(uint32_t header_raw) {
    CommandHeader h;
    h.command_id = static_cast<uint16_t>((header_raw >> 16) & 0xFFFF);
    h.param_words = static_cast<uint16_t>(header_raw & 0xFFFF);
    return h;
}

// Service Dispatcher untuk SendSyncRequest (SVC 0x32)
void DispatchServiceSync(uint32_t session_handle, uint32_t* cmd_buffer);

} // namespace IPC
} // namespace CTR
} // namespace MHXX

