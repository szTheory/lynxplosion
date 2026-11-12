/* Bounded raw-video capture and versioned software grant policy. The timer
   bank notifies this module only with the borrow mask from one application;
   sticky timer status is never used as a frame or line event source. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "internal.h"

enum {
    DISPCTL = 0xFD92u,
    PBKUP = 0xFD93u,
    DISPADDR_LO = 0xFD94u,
    DISPADDR_HI = 0xFD95u,
    VIDEO_DMA_TICKS = 5u,
    REFRESH_PERIOD = 256u,
    VISIBLE_FIRST_ORDINAL = 3u,
    VISIBLE_LAST_ORDINAL = 104u
};

static void clear_capture(lynx__video_state *video)
{
    video->row_active = 0u;
    video->capture_rows = 0u;
    video->dma_cursor = 0u;
    video->capture_partial = 0u;
    video->dma_next_tick = 0u;
    memset(video->pens, 0, sizeof video->pens);
    memset(video->palettes, 0, sizeof video->palettes);
    memset(video->line_ticks, 0, sizeof video->line_ticks);
}

/* Rows are published only after all 80 DMA bytes have arrived. A later row
   reuses this slot; clearing the dropped partial observation prevents stale
   bytes or metadata from leaking into an otherwise unused row. The bus grants
   and guest time already consumed are unaffected. */
static void discard_incomplete_row(lynx__video_state *video)
{
    const size_t row = (size_t)video->capture_rows;
    video->capture_partial = 1u;
    video->row_active = 0u;
    video->dma_cursor = 0u;
    video->dma_next_tick = 0u;
    if (row < LYNX_FRAME_MAX_ROWS) {
        memset(video->pens + row * LYNX_FRAME_ROW_BYTES, 0, LYNX_FRAME_ROW_BYTES);
        memset(video->palettes + row * 32u, 0, 32u);
        video->line_ticks[row] = 0u;
    }
}

void lynx__video_init(lynx__video_state *video, uint32_t timing_policy)
{
    if (video == NULL) {
        return;
    }
    memset(video, 0, sizeof *video);
    video->timing_policy = (uint8_t)timing_policy;
    if (timing_policy == LYNX_TIMING_POLICY_VIDEO_POLICY1) {
        video->refresh_next_tick = REFRESH_PERIOD;
    }
}

bool lynx__video_available(uint16_t addr)
{
    return addr == DISPCTL || addr == PBKUP || addr == DISPADDR_LO || addr == DISPADDR_HI ||
           (addr >= 0xFDA0u && addr <= 0xFDAFu) ||
           (addr >= 0xFDB0u && addr <= 0xFDBFu);
}

uint8_t lynx__video_peek(const lynx__video_state *video, uint16_t addr)
{
    if (video == NULL) {
        return 0u;
    }
    if (addr == DISPCTL) return video->dispctl;
    if (addr == PBKUP) return video->pbkup;
    if (addr == DISPADDR_LO) return (uint8_t)video->backup_addr;
    if (addr == DISPADDR_HI) return (uint8_t)(video->backup_addr >> 8);
    if (addr >= 0xFDA0u && addr <= 0xFDAFu) return video->palette[addr - 0xFDA0u];
    if (addr >= 0xFDB0u && addr <= 0xFDBFu) return video->palette[16u + addr - 0xFDB0u];
    return 0u;
}

bool lynx__video_write(lynx__video_state *video, uint16_t addr, uint8_t value)
{
    if (video == NULL) {
        return false;
    }
    if (addr == DISPCTL) {
        /* Only display-DMA enable is implemented; all other mode bits refuse. */
        if ((value & 0xFEu) != 0u) return false;
        if ((value & 1u) == 0u && video->row_active != 0u) {
            /* A disabled mid-row stream is abandoned. A later enable waits
               for the next Timer 0 row edge instead of replaying stale work. */
            discard_incomplete_row(video);
        }
        video->dispctl = value;
        return true;
    }
    if (addr == PBKUP) {
        video->pbkup = value;
        return true;
    }
    if (addr == DISPADDR_LO) {
        video->backup_addr = (uint16_t)((video->backup_addr & 0xFF00u) | value);
        return true;
    }
    if (addr == DISPADDR_HI) {
        video->backup_addr = (uint16_t)((video->backup_addr & 0x00FFu) |
                                        ((uint32_t)value << 8));
        return true;
    }
    if (addr >= 0xFDA0u && addr <= 0xFDAFu) {
        video->palette[addr - 0xFDA0u] = value;
        return true;
    }
    if (addr >= 0xFDB0u && addr <= 0xFDBFu) {
        video->palette[16u + addr - 0xFDB0u] = value;
        return true;
    }
    return false;
}

bool lynx__video_next_dma(const lynx__video_state *video, uint64_t now, uint64_t *tick)
{
    if (video == NULL || tick == NULL ||
        video->timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1 ||
        (video->dispctl & 1u) == 0u ||
        video->row_active == 0u || video->dma_cursor >= LYNX_FRAME_ROW_BYTES) {
        return false;
    }
    uint64_t grant = video->dma_next_tick;
    if (grant < video->dma_busy_until) grant = video->dma_busy_until;
    if (grant < video->refresh_busy_until) grant = video->refresh_busy_until;
    if (grant < now) grant = now;
    *tick = grant;
    return true;
}

bool lynx__video_next_refresh(const lynx__video_state *video, uint64_t now, uint64_t *tick)
{
    if (video == NULL || tick == NULL ||
        video->timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1 ||
        video->refresh_next_tick == 0u) {
        return false;
    }
    uint64_t grant = video->refresh_next_tick;
    if (grant < video->dma_busy_until) grant = video->dma_busy_until;
    if (grant < video->refresh_busy_until) grant = video->refresh_busy_until;
    if (grant < now) grant = now;
    *tick = grant;
    return true;
}

void lynx__video_dma_apply(lynx__video_state *video, const uint8_t *ram, uint64_t tick)
{
    if (video == NULL || ram == NULL ||
        video->timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1 || video->row_active == 0u ||
        (video->dispctl & 1u) == 0u || video->capture_rows >= LYNX_FRAME_MAX_ROWS ||
        video->dma_cursor >= LYNX_FRAME_ROW_BYTES || tick < video->dma_next_tick ||
        tick < video->dma_busy_until || tick < video->refresh_busy_until ||
        tick > UINT64_MAX - VIDEO_DMA_TICKS) {
        return;
    }
    const size_t row = (size_t)video->capture_rows;
    video->pens[row * LYNX_FRAME_ROW_BYTES + video->dma_cursor] = ram[video->dispaddr];
    video->dispaddr++;
    video->dma_cursor++;
    video->dma_busy_until = tick + VIDEO_DMA_TICKS;
    if (video->dma_cursor == LYNX_FRAME_ROW_BYTES) {
        video->row_active = 0u;
        video->dma_cursor = 0u;
        video->dma_next_tick = 0u;
        video->capture_rows++;
    } else {
        video->dma_next_tick = tick + VIDEO_DMA_TICKS;
    }
}

void lynx__video_refresh_apply(lynx__video_state *video, uint64_t tick)
{
    if (video == NULL || video->refresh_next_tick == 0u ||
        tick < video->refresh_next_tick || tick < video->dma_busy_until ||
        tick < video->refresh_busy_until || tick > UINT64_MAX - VIDEO_DMA_TICKS) {
        return;
    }
    const uint64_t previous_deadline = video->refresh_next_tick;
    video->refresh_busy_until = tick + VIDEO_DMA_TICKS;
    const uint64_t elapsed = tick - previous_deadline;
    const uint64_t periods = elapsed / REFRESH_PERIOD + 1u;
    video->refresh_next_tick = periods <= (UINT64_MAX - previous_deadline) / REFRESH_PERIOD ?
                                   previous_deadline + periods * REFRESH_PERIOD : 0u;
}

static void begin_visible_row(lynx__video_state *video, uint64_t tick)
{
    if ((video->dispctl & 1u) == 0u || video->capture_rows >= LYNX_FRAME_MAX_ROWS) {
        return;
    }
    if (video->row_active != 0u) {
        discard_incomplete_row(video);
    }
    const size_t row = (size_t)video->capture_rows;
    /* MUTATION_ANCHOR: video.palette-row-snapshot */
    memcpy(video->palettes + row * 32u, video->palette, 32u);
    video->line_ticks[row] = tick;
    video->row_active = 1u;
    video->dma_cursor = 0u;
    video->dma_next_tick = tick <= UINT64_MAX - VIDEO_DMA_TICKS ?
                               tick + VIDEO_DMA_TICKS : UINT64_MAX;
}

bool lynx__video_timer_apply(lynx__video_state *video, uint8_t borrow_mask, uint64_t tick)
{
    if (video == NULL || video->timing_policy != LYNX_TIMING_POLICY_VIDEO_POLICY1) {
        return false;
    }
    const bool timer2_underflow = (borrow_mask & (1u << 2)) != 0u;
    const bool timer0_underflow = (borrow_mask & (1u << 0)) != 0u;
    if (timer2_underflow) {
        if (video->row_active != 0u) {
            discard_incomplete_row(video);
        }
        video->frame_end_tick = tick;
        if (video->frame_synced != 0u) {
            video->frame_sequence++;
        } else {
            video->frame_synced = 1u;
            video->frame_sequence = 0u;
        }
        if (timer0_underflow) {
            video->line_ordinal = 0u;
        }
    }
    if (timer0_underflow && video->frame_synced != 0u) {
        if (!timer2_underflow && video->line_ordinal < VISIBLE_LAST_ORDINAL) {
            video->line_ordinal++;
        }
        if (video->line_ordinal == 2u) {
            /* MUTATION_ANCHOR: video.display-address-select */
            video->dispaddr = (uint16_t)(video->backup_addr & 0xFFFCu);
        }
        if (video->line_ordinal >= VISIBLE_FIRST_ORDINAL &&
            video->line_ordinal <= VISIBLE_LAST_ORDINAL) {
            begin_visible_row(video, tick);
        }
    }
    return timer2_underflow;
}

void lynx__video_copy_frame(const lynx__video_state *video,
                            const lynx_frame_buffers *buffers)
{
    if (video == NULL || buffers == NULL) {
        return;
    }
    memcpy(buffers->pens, video->pens, sizeof video->pens);
    memcpy(buffers->palettes, video->palettes, sizeof video->palettes);
    memcpy(buffers->line_ticks, video->line_ticks, sizeof video->line_ticks);
}

void lynx__video_discard_frame(lynx__video_state *video, uint64_t next_start_tick)
{
    if (video == NULL) {
        return;
    }
    clear_capture(video);
    video->frame_start_tick = next_start_tick;
    video->frame_end_tick = 0u;
}
