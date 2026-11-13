; Project-owned timer evidence wrapper around the unchanged pinned tests.s.
        .setcpu "65C02"
        .include "tests.s"

        .segment "ZEROPAGE"
suite_probe_result: .res 1
suite_text_ptr:     .res 2
suite_frame_ptr:    .res 2
suite_glyph_ptr:    .res 2

        .segment "BLL"
        .export suite_end
bll_header:
        .byte $80, $08, $02, $00
        .byte >(suite_end - bll_header), <(suite_end - bll_header)
        .byte "BS93"

        .export suite_entry, suite_test_entry, suite_test_return
        .export suite_after_tests, suite_call_tests, suite_default_irq
        .export suite_result_start, suite_result_end
        .export suite_progress, suite_model, suite_done
        .export suite_requested_dispctl, suite_requested_mapctl
        .export suite_initial_counter, suite_completion_signature
        .export suite_framebuffer_start, suite_framebuffer_end
        .export suite_test6, suite_test6_poll
        .export suite_counter_zp
        suite_test_entry = _run_tests
        suite_test_return = _run_tests + 26
        suite_result_start = _g_results
        suite_result_end = _g_results + 18
        suite_test6 = Test6
        ; The builder independently checks this source-derived offset.
        suite_test6_poll = $0400
        suite_counter_zp = $01

        .segment "STARTUP"
suite_entry:
        jmp suite_extension_entry
        .res $45 - (* - suite_entry), $EA
suite_call_tests:
        jsr _run_tests
suite_after_tests:
        jmp suite_extension_after_tests
        .res $73 - (* - suite_entry), $EA

        .segment "EXTENSION"
suite_extension_entry:
        sei
        ldx #$FF
        txs

        lda #$00
        ldx #$00
@clear_zp:
        sta $00,x
        inx
        bne @clear_zp
        ldx #17
@clear_results:
        sta _g_results,x
        dex
        bpl @clear_results
        stz suite_progress
        stz suite_model
        stz suite_done
        stz suite_completion_signature

        ; Independent Lynx I/Lynx II CPU feature probe used by the wrapper.
        lda #$01
        sta suite_probe_result
        .byte $07, <suite_probe_result
        lda suite_probe_result
        bne @probe_lynx1
        lda #$A2
        bra @store_model
@probe_lynx1:
        lda #$A1
@store_model:
        sta suite_model

        ; Select RAM overlays before installing the RAM IRQ vector.
        lda #MAPCTL_REQUESTED
        sta suite_requested_mapctl
        sta MAPCTL

        lda #<suite_default_irq
        sta INTVECTL
        lda #>suite_default_irq
        sta INTVECTH

        lda #$00
        sta TIM0CTLA
        sta TIM2CTLA
        lda #$FF
        sta INTRST
        lda #$9E
        sta TIM0BKUP
        lda #$68
        sta TIM2BKUP
        lda #$98
        sta TIM0CTLA
        lda #$9F
        sta TIM2CTLA

        lda #<$6000
        sta DISPADRL
        lda #>$6000
        sta DISPADRH
        lda #$29
        sta PBKUP
        lda #$0F
        sta $FDA1
        lda #$FF
        sta $FDB1

        lda #DISPCTL_REQUESTED
        sta suite_requested_dispctl
        lda #$00
        sta suite_initial_counter
        sta suite_counter_zp
        lda #$01
        sta suite_progress
        lda #DISPCTL_REQUESTED
suite_dispctl_request:
        sta DISPCTL
suite_dispctl_after:
        lda #(TIMER0_INTERRUPT | TIMER2_INTERRUPT)
        sta INTSET
        jmp suite_call_tests

suite_extension_after_tests:
        sei
        lda #$08
        sta suite_progress
        jsr suite_clear_framebuffer
        jsr suite_draw_header
        jsr suite_draw_results

        ; Requested DISPCTL values are recorded separately from core support.
        ; Software companions restore one-bit DMA for readable capture.
        lda #$01
        sta DISPCTL
        lda #$FF
        sta INTRST
        lda #(TIMER0_INTERRUPT | TIMER2_INTERRUPT)
        sta INTSET
        ; At this point the captured source results and framebuffer bytes are
        ; complete and the readable display mode is restored. The host checks
        ; a separately captured raw frame before accepting the tracer.
        lda #$A5
        sta suite_completion_signature
        sta suite_done
@idle:
        jmp @idle

suite_clear_framebuffer:
        lda #$00
        sta suite_frame_ptr
        lda #$60
        sta suite_frame_ptr+1
        ldx #31
@clear_full_pages:
        ldy #$00
@clear_page:
        sta (suite_frame_ptr),y
        iny
        bne @clear_page
        inc suite_frame_ptr+1
        dex
        bne @clear_full_pages
        ldx #$E0
@clear_tail:
        sta (suite_frame_ptr),y
        iny
        dex
        bne @clear_tail
        rts

suite_draw_header:
        lda #$00
        sta suite_draw_x
        sta suite_draw_y
        lda #<suite_line_one
        sta suite_text_ptr
        lda #>suite_line_one
        sta suite_text_ptr+1
        jsr suite_draw_text
        lda #$00
        sta suite_draw_x
        lda #$06
        sta suite_draw_y
        lda #<suite_line_two
        sta suite_text_ptr
        lda #>suite_line_two
        sta suite_text_ptr+1
        jmp suite_draw_text

suite_draw_text:
@next_char:
        ldy #$00
        lda (suite_text_ptr),y
        beq @text_done
        sta suite_draw_char
        inc suite_text_ptr
        bne @text_ptr_ready
        inc suite_text_ptr+1
@text_ptr_ready:
        jsr suite_draw_char_pixels
        bra @next_char
@text_done:
        rts

suite_draw_results:
        ldx #$00
@result_row:
        stx suite_result_index
        lda #$00
        sta suite_draw_x
        txa
        sta suite_result_row
        asl
        asl
        clc
        adc suite_result_row
        clc
        adc suite_result_row
        clc
        adc #$0E
        sta suite_draw_y
        jsr suite_draw_result
        ldx suite_result_index
        inx
        cpx #$09
        bne @result_row
        ldx #$09
@result_right:
        stx suite_result_index
        lda #$0A
        sta suite_draw_x
        txa
        sec
        sbc #$09
        sta suite_result_row
        asl
        asl
        clc
        adc suite_result_row
        clc
        adc suite_result_row
        clc
        adc #$0E
        sta suite_draw_y
        jsr suite_draw_result
        ldx suite_result_index
        inx
        cpx #$12
        bne @result_right
        rts

suite_draw_result:
        lda suite_result_index
        sta suite_draw_value
        jsr suite_draw_hex_byte
        lda #'='
        sta suite_draw_char
        jsr suite_draw_char_pixels
        ldx suite_result_index
        lda _g_results,x
        sta suite_draw_value
        jmp suite_draw_hex_byte

suite_draw_hex_byte:
        lda suite_draw_value
        lsr
        lsr
        lsr
        lsr
        tax
        lda suite_hex_digits,x
        sta suite_draw_char
        jsr suite_draw_char_pixels
        lda suite_draw_value
        and #$0F
        tax
        lda suite_hex_digits,x
        sta suite_draw_char
        jmp suite_draw_char_pixels

suite_draw_char_pixels:
        lda suite_draw_char
        cmp #'='
        beq @equal
        cmp #' '
        beq @space
        cmp #'0'
        bcc @blank
        cmp #':'
        bcc @digit
        cmp #'A'
        bcc @blank
        cmp #'Z'+1
        bcs @blank
        sec
        sbc #'A'
        clc
        adc #$0A
        bra @glyph_ready
@digit:
        sec
        sbc #'0'
        bra @glyph_ready
@equal:
        lda #$24
        bra @glyph_ready
@space:
        lda #$25
        bra @glyph_ready
@blank:
        lda #$25
@glyph_ready:
        sta suite_glyph_index
        lda #<suite_glyphs
        sta suite_glyph_ptr
        lda #>suite_glyphs
        sta suite_glyph_ptr+1
        ldx suite_glyph_index
        beq @glyph_address_ready
@glyph_address:
        clc
        lda suite_glyph_ptr
        adc #10
        sta suite_glyph_ptr
        bcc @glyph_no_carry
        inc suite_glyph_ptr+1
@glyph_no_carry:
        dex
        bne @glyph_address
@glyph_address_ready:
        lda #<suite_framebuffer_start
        sta suite_frame_ptr
        lda #>suite_framebuffer_start
        sta suite_frame_ptr+1
        ldx suite_draw_y
        beq @base_row_ready
@base_row:
        clc
        lda suite_frame_ptr
        adc #80
        sta suite_frame_ptr
        bcc @base_no_carry
        inc suite_frame_ptr+1
@base_no_carry:
        dex
        bne @base_row
@base_row_ready:
        ldx suite_draw_x
        beq @char_offset_ready
@char_offset:
        clc
        lda suite_frame_ptr
        adc #3
        sta suite_frame_ptr
        bcc @char_no_carry
        inc suite_frame_ptr+1
@char_no_carry:
        dex
        bne @char_offset
@char_offset_ready:
        ldx #$05
@draw_glyph_row:
        ldy #$00
        lda (suite_glyph_ptr),y
        sta (suite_frame_ptr),y
        inc suite_glyph_ptr
        bne @glyph_second_ready
        inc suite_glyph_ptr+1
@glyph_second_ready:
        lda (suite_glyph_ptr),y
        iny
        sta (suite_frame_ptr),y
        inc suite_glyph_ptr
        bne @glyph_row_pointer_ready
        inc suite_glyph_ptr+1
@glyph_row_pointer_ready:
        lda #$00
        iny
        sta (suite_frame_ptr),y
        clc
        lda suite_frame_ptr
        adc #80
        sta suite_frame_ptr
        bcc @frame_row_no_carry
        inc suite_frame_ptr+1
@frame_row_no_carry:
        dex
        bne @draw_glyph_row
        inc suite_draw_x
        rts

suite_default_irq:
        pha
        txa
        pha
        tsx
        lda $0104,x
        cmp #<suite_after_tests
        bne @irq_done
        lda $0105,x
        cmp #>suite_after_tests
        bne @irq_done
        lda $0103,x
        ora #$04
        sta $0103,x
@irq_done:
        pla
        tax
        pla
        rti

        .segment "BSS"
suite_progress:               .res 1
suite_model:                  .res 1
suite_done:                   .res 1
suite_requested_dispctl:      .res 1
suite_requested_mapctl:       .res 1
suite_initial_counter:        .res 1
suite_completion_signature:   .res 1
suite_draw_char:              .res 1
suite_draw_x:                 .res 1
suite_draw_y:                 .res 1
suite_draw_value:             .res 1
suite_result_index:           .res 1
suite_result_row:             .res 1
suite_glyph_index:            .res 1
suite_clear_pages:            .res 1

        .segment "FRAMEBUFFER"
suite_framebuffer_start:
        .res 8160
suite_framebuffer_end:

        .segment "EXTENSION"
suite_hex_digits:
        .byte "0123456789ABCDEF"
suite_line_one:
        .byte "TIMER D", $30 + DMA, "P", $30 + PAGE, " "
.if FAMILY_HARDWARE
        .byte "H0D", $00
.else
        .byte "S01", $00
.endif
suite_line_two:
        .byte "INIT00 DONEA5", $00
suite_glyphs:
        ; 0
        .byte $0f, $f0, $f0, $0f, $f0, $0f, $f0, $0f, $0f, $f0
        ; 1
        .byte $00, $f0, $0f, $f0, $00, $f0, $00, $f0, $0f, $ff
        ; 2
        .byte $0f, $f0, $f0, $0f, $00, $f0, $0f, $00, $ff, $ff
        ; 3
        .byte $ff, $f0, $00, $0f, $0f, $f0, $00, $0f, $ff, $f0
        ; 4
        .byte $f0, $0f, $f0, $0f, $ff, $ff, $00, $0f, $00, $0f
        ; 5
        .byte $ff, $ff, $f0, $00, $ff, $f0, $00, $0f, $ff, $f0
        ; 6
        .byte $0f, $f0, $f0, $00, $ff, $f0, $f0, $0f, $0f, $f0
        ; 7
        .byte $ff, $ff, $00, $0f, $00, $f0, $0f, $00, $0f, $00
        ; 8
        .byte $0f, $f0, $f0, $0f, $0f, $f0, $f0, $0f, $0f, $f0
        ; 9
        .byte $0f, $f0, $f0, $0f, $0f, $ff, $00, $0f, $0f, $f0
        ; A
        .byte $0f, $f0, $f0, $0f, $ff, $ff, $f0, $0f, $f0, $0f
        ; B
        .byte $ff, $f0, $f0, $0f, $ff, $f0, $f0, $0f, $ff, $f0
        ; C
        .byte $0f, $ff, $f0, $00, $f0, $00, $f0, $00, $0f, $ff
        ; D
        .byte $ff, $f0, $f0, $0f, $f0, $0f, $f0, $0f, $ff, $f0
        ; E
        .byte $ff, $ff, $f0, $00, $ff, $f0, $f0, $00, $ff, $ff
        ; F
        .byte $ff, $ff, $f0, $00, $ff, $f0, $f0, $00, $f0, $00
        ; G
        .byte $0f, $ff, $f0, $00, $f0, $ff, $f0, $0f, $0f, $ff
        ; H
        .byte $f0, $0f, $f0, $0f, $ff, $ff, $f0, $0f, $f0, $0f
        ; I
        .byte $0f, $ff, $00, $f0, $00, $f0, $00, $f0, $0f, $ff
        ; J
        .byte $00, $0f, $00, $0f, $00, $0f, $f0, $0f, $0f, $f0
        ; K
        .byte $f0, $0f, $f0, $f0, $ff, $00, $f0, $f0, $f0, $0f
        ; L
        .byte $f0, $00, $f0, $00, $f0, $00, $f0, $00, $ff, $ff
        ; M
        .byte $f0, $0f, $ff, $ff, $ff, $ff, $f0, $0f, $f0, $0f
        ; N
        .byte $f0, $0f, $ff, $0f, $f0, $ff, $f0, $0f, $f0, $0f
        ; O
        .byte $0f, $f0, $f0, $0f, $f0, $0f, $f0, $0f, $0f, $f0
        ; P
        .byte $ff, $f0, $f0, $0f, $ff, $f0, $f0, $00, $f0, $00
        ; Q
        .byte $0f, $f0, $f0, $0f, $f0, $0f, $f0, $ff, $0f, $ff
        ; R
        .byte $ff, $f0, $f0, $0f, $ff, $f0, $f0, $f0, $f0, $0f
        ; S
        .byte $0f, $ff, $f0, $00, $0f, $f0, $00, $0f, $ff, $f0
        ; T
        .byte $ff, $ff, $00, $f0, $00, $f0, $00, $f0, $00, $f0
        ; U
        .byte $f0, $0f, $f0, $0f, $f0, $0f, $f0, $0f, $0f, $f0
        ; V
        .byte $f0, $0f, $f0, $0f, $f0, $0f, $0f, $f0, $0f, $f0
        ; W
        .byte $f0, $0f, $f0, $0f, $ff, $ff, $ff, $ff, $f0, $0f
        ; X
        .byte $f0, $0f, $0f, $f0, $0f, $f0, $0f, $f0, $f0, $0f
        ; Y
        .byte $f0, $0f, $0f, $f0, $00, $f0, $00, $f0, $00, $f0
        ; Z
        .byte $ff, $ff, $00, $0f, $00, $f0, $0f, $00, $ff, $ff
        ; =
        .byte $00, $00, $ff, $ff, $00, $00, $ff, $ff, $00, $00
        ; Space follows the equals glyph above.
        .byte $00,$00, $00,$00, $00,$00, $00,$00, $00,$00
        .export suite_pinned_code_start
        .export suite_dispctl_request, suite_dispctl_after
suite_pinned_code_start = $0273
suite_end:

