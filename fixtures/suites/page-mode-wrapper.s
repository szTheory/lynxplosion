; Original wrapper around the unchanged pinned page-mode/tests.s suite.
        .setcpu "65C02"
        .include "tests.s"

        .segment "ZEROPAGE"
suite_probe_result: .res 1

        .segment "BLL"
        .export suite_end
bll_header:
        .byte $80, $08, $02, $00
        .byte >(suite_end - bll_header), <(suite_end - bll_header)
        .byte "BS93"

        .export suite_entry, suite_test_entry, suite_result_end
        .export suite_progress, suite_model, suite_done
        .export suite_first_test, suite_first_mapped, suite_probe_result
        .export suite_after_tests
        suite_test_entry = _run_tests
        suite_result_end = _g_results + 6
        suite_first_test = Test1_NOP_PageOn
        suite_first_mapped = Test1_NOP_PageOn

        .segment "STARTUP"
suite_entry:
        sei
        ldx #$FF
        txs

        ; The pinned WaitVBlank polls Timer2. Source-derived 60 Hz timer
        ; setup: Timer0 runs from source 0 at backup 158; Timer2 links to it
        ; at backup 104. This is software environment setup, not a hardware
        ; timing qualification.
        lda #$0C
        sta MAPCTL
        lda #$9E
        sta TIM0BKUP
        sta TIM0CNT
        lda #$68
        sta TIM2BKUP
        sta TIM2CNT
        lda #$18
        sta TIM0CTLA
        lda #$1F
        sta TIM2CTLA

        ; Clear pinned zero-page scratch, the six-byte result buffer, and
        ; wrapper state before entering the unchanged source tests.
        lda #$00
        ldx #$00
@clear_zp:
        sta $00,x
        inx
        bne @clear_zp
        ldx #5
@clear_results:
        sta _g_results,x
        dex
        bpl @clear_results
        stz suite_progress
        stz suite_model
        stz suite_done

        ; Detect the executing CPU independently of the selected host profile.
        lda #$01
        sta suite_probe_result
        .byte $07, <suite_probe_result ; RMB0 zp on Lynx II, NOP zp on Lynx I
        lda suite_probe_result
        bne @probe_lynx1
        lda #$A2
        bra @store_model
@probe_lynx1:
        lda #$A1
@store_model:
        sta suite_model

        lda #$01
        sta suite_progress
        jsr _run_tests
suite_after_tests:
        lda #$08
        sta suite_progress
        lda #$A5
        sta suite_done
suite_idle:
        jmp suite_idle

        .segment "BSS"
suite_progress: .res 1
suite_model:    .res 1
suite_done:     .res 1

        .segment "CODE"
suite_end:
