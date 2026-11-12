; Small original-guest tracer retained for the Phase 5 released/A baseline.
; It keeps the historical first-pen A assertion while the v3 probe expands
; the complete input matrix in input_probe.s.
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
        sta $FFF9
        lda #$08
        sta $FC92

        lda #$00
        sta $FD00
        sta $FD02
        lda #$1D
        sta $FD01
        lda #$69
        sta $FD08
        sta $FD0A
        lda #$1F
        sta $FD09

        lda #$00
        sta $FD93
        sta $FD94
        lda #$40
        sta $FD95
        lda #$0F
        sta $FDA1
        lda #$00
        sta $FDB1
        sta $FDA2
        lda #$0F
        sta $FDB2
        lda #$01
        sta $FD92

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
        sta probe_result + 5
        sta probe_result + 6
        lda #$40
        sta probe_result + 7

poll_input:
        lda $FCB0
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
        sta probe_result + 7
progress_saturated:
        lda probe_result + 5
        and #$01
        beq released
        lda #$22
        sta $4000
        jmp poll_input
released:
        lda #$11
        sta $4000
        jmp poll_input

probe_end:

        .segment "RESULT"
probe_result:
        .res 8
