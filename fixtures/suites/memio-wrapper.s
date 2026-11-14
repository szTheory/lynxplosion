; Project-authored returning wrapper around unchanged pinned memio/tests.s.
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
        .export suite_test_return, suite_after_tests
        suite_test_entry = _run_tests
        ; The pinned runner is SEI, three test JSRs, CLI, RTS.
        suite_test_return = _run_tests + 11
        suite_result_end = _g_results + 18
        suite_first_test = Test1
        suite_first_mapped = Test1

        .segment "STARTUP"
suite_entry:
        sei
        ldx #$FF
        txs
        lda #$0C
        sta MAPCTL

        ; Clear guest scratch, the full source result extent and wrapper state.
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

        ; Detect the executing CPU using its RMB0 behavior.
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
