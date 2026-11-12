/* Logical input readback at the CPU bus sample point. Physical controller
   discovery, switch polarity and disconnected cartridge lines are host policy
   and outside this instance-owned nine-bit level mask. */
#include <stdbool.h>
#include <stdint.h>

#include "internal.h"

enum { SPRSYS = 0xFC92u, JOYSTICK = 0xFCB0u, SWITCHES = 0xFCB1u };

lynx_status lynx__input_set(lynx__video_state *video, uint32_t mask)
{
    if (video == NULL || (mask & ~(uint32_t)LYNX_INPUT_MASK) != 0u) {
        return LYNX_ERR_ARGUMENT;
    }
    video->input_mask = (uint16_t)mask;
    return LYNX_OK;
}

bool lynx__input_available(uint16_t addr)
{
    return addr == SPRSYS || addr == JOYSTICK || addr == SWITCHES;
}

uint8_t lynx__input_peek(const lynx__video_state *video, uint16_t addr)
{
    if (video == NULL) {
        return 0u;
    }
    if (addr == SPRSYS) {
        return video->lefthand != 0u ? 0x08u : 0u;
    }
    if (addr == SWITCHES) {
        return (video->input_mask & LYNX_INPUT_PAUSE) != 0u ? 0x01u : 0u;
    }
    if (addr == JOYSTICK) {
        const uint16_t mask = video->input_mask;
        uint8_t value = 0u;
        if (video->lefthand != 0u) {
            if ((mask & LYNX_INPUT_UP) != 0u) value |= 0x80u;
            if ((mask & LYNX_INPUT_DOWN) != 0u) value |= 0x40u;
            if ((mask & LYNX_INPUT_LEFT) != 0u) value |= 0x20u;
            if ((mask & LYNX_INPUT_RIGHT) != 0u) value |= 0x10u;
        } else {
            if ((mask & LYNX_INPUT_UP) != 0u) value |= 0x40u;
            if ((mask & LYNX_INPUT_DOWN) != 0u) value |= 0x80u;
            if ((mask & LYNX_INPUT_LEFT) != 0u) value |= 0x10u;
            if ((mask & LYNX_INPUT_RIGHT) != 0u) value |= 0x20u;
        }
        if ((mask & LYNX_INPUT_OPTION1) != 0u) value |= 0x08u;
        if ((mask & LYNX_INPUT_OPTION2) != 0u) value |= 0x04u;
        if ((mask & LYNX_INPUT_B) != 0u) value |= 0x02u;
        if ((mask & LYNX_INPUT_A) != 0u) value |= 0x01u;
        return value;
    }
    return 0u;
}

bool lynx__input_write(lynx__video_state *video, uint16_t addr, uint8_t value)
{
    if (video == NULL || addr != SPRSYS || (value & 0xF7u) != 0u) {
        return false;
    }
    video->lefthand = (uint8_t)((value & 0x08u) != 0u);
    return true;
}
