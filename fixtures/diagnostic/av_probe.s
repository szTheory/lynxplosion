; Original input-responsive A/V tracer for the public API.
; The input marker is guest-authored; channel-0 output follows a digital
; software divider/LFSR policy and is not hardware qualification evidence.
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
        sta probe_result + 7
        sta probe_result + 8

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
        lda probe_result + 8
        bne poll_input
        lda #$01
        sta probe_result + 8
        lda #$01
        sta $FD20
        sta $FD21
        sta $FD23
        lda #$07
        sta $FD24
        lda #$18
        sta $FD25
        jmp poll_input
released:
        lda #$11
        sta $4000
        lda probe_result + 8
        beq poll_input
        lda #$00
        sta $FD25
        sta $FD20
        sta probe_result + 8
        jmp poll_input

probe_end:

        .segment "RESULT"
probe_result:
        .res 9
