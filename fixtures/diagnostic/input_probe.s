; Original Phase 5 input/display tracer. It is direct-load BS93 source, not
; derived from a commercial title or an emulator implementation.
        .setcpu "65SC02"
        .segment "BLL"
        .export start
        .export probe_result

bll_header:
        .byte $80, $08, $02, $00
        .byte >(probe_end - bll_header), <(probe_end - bll_header)
        .byte "BS93"

start:
        lda #$0C
        sta $FFF9             ; Suzy/Mikey enabled, ROM/vector windows RAM.
        lda #$08
        sta $FC92             ; Explicit Lefthand orientation.

        lda #$00
        sta $FD00             ; Timer 0 reload value.
        sta $FD02             ; Timer 0 initial count.
        lda #$1D              ; Count + reload, source 5 (512 guest ticks).
        sta $FD01
        lda #$69
        sta $FD08             ; Timer 2 reload after 105 line links.
        sta $FD0A
        lda #$1F              ; Timer 2 linked to timer 0, count + reload.
        sta $FD09

        lda #$00
        sta $FD93             ; PBKUP policy: no flip.
        sta $FD94             ; DISPADDR low.
        lda #$40
        sta $FD95             ; DISPADDR high: framebuffer at $4000.
        lda #$0F
        sta $FDA1             ; Pen 1 green nibble.
        lda #$00
        sta $FDB1             ; Pen 1 blue/red nibbles.
        sta $FDA2             ; Pen 2 green nibble.
        lda #$0F
        sta $FDB2             ; Pen 2 red nibble.
        lda #$01
        sta $FD92             ; Enable the documented display-DMA subset.

        lda #'L'
        sta probe_result
        lda #'X'
        sta probe_result + 1
        lda #'I'
        sta probe_result + 2
        lda #'5'
        sta probe_result + 3
        lda #$01
        sta probe_result + 4

poll_input:
        lda $FCB0             ; Guest samples the logical joystick register.
        and #$01              ; A is the low bit in the project mapping.
        beq released
        lda #$01
        sta probe_result + 5
        lda #$22              ; Two pen-2 pixels in GREEN/BLUERED raw order.
        sta $4000
        lda #$02
        sta probe_result + 4
        jmp poll_input
released:
        lda #$00
        sta probe_result + 5
        lda #$11              ; Two pen-1 pixels while A is released.
        sta $4000
        lda #$02
        sta probe_result + 4
        jmp poll_input

probe_end:

        .segment "RESULT"
probe_result:
        .res 8
