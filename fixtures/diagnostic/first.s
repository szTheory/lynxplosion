; Original Phase 3 tier-A diagnostic for profile none.
; The first ten output bytes are the canonical project BS93 header. The
; linker places this segment at $01F6, so start (and the declared BLL payload
; address) is $0200. diag_result and diag_pixels live in uninitialized RAM,
; not in the BLL payload. No CRT, TGI, boot code, or emulator source is used.
; Manual normal-path trace, written before running the guest: MAPCTL 6 cycles,
; result setup 42, three RAM-window checks 63, progress 2 is 6, the pixel loop
; is 2 + 31*16 + 15 = 513, progress 3 is 6, and done is 6: 642 cycles total.
; Flat5 maps this to 3,210 nominal master ticks. The host uses fixed 3,000 to
; 4,000 tick bounds around this hand count; flat5 is a PLACEHOLDER, not Lynx
; hardware timing evidence.

        .setcpu "65SC02"
        .ifndef DIAG_FAULT
DIAG_FAULT = 0
        .endif

        .segment "BLL"
        .export start
        .export diag_result
        .export diag_pixels
        .export diag_pixels_end

bll_header:
        .byte $80, $08, $02, $00
        .byte >(diagnostic_end - bll_header), <(diagnostic_end - bll_header)
        .byte "BS93"

start:
        ; Tier A enters with MAPCTL zero. Guest code selects RAM for ROM and
        ; vectors before testing their windows.
        lda #$0C
        sta $FFF9

        lda #'L'
        sta diag_result
        lda #'X'
        sta diag_result + 1
        lda #'D'
        sta diag_result + 2
        lda #'3'
        sta diag_result + 3
        lda #$01
        sta diag_result + 4
        lda #$01
        sta diag_result + 5
        lda #$00
        sta diag_result + 6

        ; Check ordinary RAM, selected ROM-window RAM, and selected vector
        ; RAM. Restore every byte this diagnostic owns to its zero load value.
        lda #$5A
        sta $0600
        lda $0600
        cmp #$5A
        beq ram_ok
        lda #$01
        sta diag_result + 6
ram_ok:
        lda #$00
        sta $0600

        lda #$A6
        sta $FE00
        lda $FE00
        cmp #$A6
        beq rom_ram_ok
        lda #$01
        sta diag_result + 6
rom_ram_ok:
        lda #$00
        sta $FE00

        lda #$5C
        sta $FFFA
        lda $FFFA
        cmp #$5C
        beq vector_ram_ok
        lda #$01
        sta diag_result + 6
vector_ram_ok:
        lda #$00
        sta $FFFA
        lda #$02
        sta diag_result + 5

        ; Each table byte is the independently specified packed pair for the
        ; 8x8 formula pixel(x,y)=(x+2*y)&15, high nibble first.
        ldx #$00
pixel_loop:
        lda pixel_values,x
        sta diag_pixels,x
        inx
        cpx #$20
        bne pixel_loop

        lda #$03
        sta diag_result + 5
        .if DIAG_FAULT = 1
        lda #$01
        sta diag_result + 6
        .endif

        ; This variant executes real guest instructions but never writes done.
        .if DIAG_FAULT = 2
timeout_loop:
        jmp timeout_loop
        .endif

        ; The completion marker is the last guest RAM write.
        lda #$A5
        sta diag_result + 7
stable_loop:
        jmp stable_loop

pixel_values:
        .byte $01,$23,$45,$67, $23,$45,$67,$89
        .byte $45,$67,$89,$AB, $67,$89,$AB,$CD
        .byte $89,$AB,$CD,$EF, $AB,$CD,$EF,$01
        .byte $CD,$EF,$01,$23, $EF,$01,$23,$45

diagnostic_end:

        .segment "RESULT"
diag_result:
        .res 8

        .segment "PIXELS"
diag_pixels:
        .res 32
diag_pixels_end:
