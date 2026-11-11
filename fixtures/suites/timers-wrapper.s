; Original wrapper around the unchanged pinned timers/tests.s suite.
        .setcpu "65C02"
        .include "tests.s"

        .segment "BLL"
        .export suite_end
bll_header:
        .byte $80, $08, $02, $00
        .byte >(suite_end - bll_header), <(suite_end - bll_header)
        .byte "BS93"

        .export suite_entry, suite_test_entry, suite_result_end
        .export suite_progress, suite_model, suite_done
        .export suite_first_test, suite_first_mapped
        suite_test_entry = _run_tests
        suite_result_end = _g_results + 18
        suite_first_test = Test1
        suite_first_mapped = ResetTimers

        .segment "STARTUP"
suite_entry:
        sei
        ldx #$FF
        txs

        ; Clear pinned zero-page scratch, the reserved 18-byte result buffer,
        ; and wrapper state before entering the source tests.
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

        lda #$0C
        sta MAPCTL
        lda #<suite_default_irq
        sta INTVECTL
        lda #>suite_default_irq
        sta INTVECTH

        lda #$01
        sta suite_progress
        jsr _run_tests
        lda #$A5
        sta suite_done
suite_idle:
        jmp suite_idle

suite_default_irq:
        rti

        .segment "BSS"
suite_progress: .res 1
suite_model:    .res 1
suite_done:     .res 1

        .segment "CODE"
suite_end:
