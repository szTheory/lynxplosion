; Original Phase 5 input/display tracer. It is direct-load BS93 source, not
; derived from a commercial title or an emulator implementation.
        .setcpu "65SC02"
        .ifndef LEFTHAND
LEFTHAND = 1
        .endif
        .segment "BLL"
        .export start
        .export probe_result
        .export probe_lefthand

bll_header:
        .byte $80, $08, $02, $00
        .byte >(probe_end - bll_header), <(probe_end - bll_header)
        .byte "BS93"

start:
        lda #$0C
        sta $FFF9             ; Suzy/Mikey enabled, ROM/vector windows RAM.
        .if LEFTHAND
        lda #$08
        .else
        lda #$00
        .endif
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
        lda #$03
        sta probe_result + 4
        lda #$00
        sta probe_result + 5 ; Latest guest JOYSTICK sample.
        sta probe_result + 6 ; Latest guest SWITCHES sample.
        lda #$40
        sta probe_result + 7 ; Done=1, error=0, saturated poll progress starts at 0.

poll_input:
        lda $FCB0             ; Guest samples JOYSTICK and SWITCHES at its own bus grants.
        sta probe_result + 5
        lda $FCB1
        sta probe_result + 6
        lda probe_result + 7
        and #$3F
        cmp #$3F
        beq progress_saturated
        clc
        adc #$01
        ora #$40
        sta probe_result + 7 ; Low six bits saturate; bit 6 is done, bit 7 is error.
progress_saturated:
        ldx #$00
pattern_loop:
        lda input_masks,x
        and probe_result + 5
        beq pattern_released
        lda #$22              ; Two pen-2 pixels for a held logical control.
        bne pattern_store
pattern_released:
        lda #$11              ; Two pen-1 pixels for a released logical control.
pattern_store:
        sta $4000,x
        inx
        cpx #$08
        bne pattern_loop
        lda probe_result + 6
        and #$01
        beq pause_released
        lda #$22
        bne pause_store
pause_released:
        lda #$11
pause_store:
        sta $4008
        jmp poll_input

input_masks:
        .if LEFTHAND
        .byte $01,$02,$80,$40,$20,$10,$08,$04
        .else
        .byte $01,$02,$40,$80,$10,$20,$08,$04
        .endif
probe_lefthand:
        .byte LEFTHAND

probe_end:

        .segment "RESULT"
probe_result:
        .res 8
