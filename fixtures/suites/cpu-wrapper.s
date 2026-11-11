; Original wrapper around the unchanged pinned cpu/tests.s suite.
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
        .export suite_first_test, suite_first_mapped
        .export suite_probe_result
        suite_test_entry = _run_tests
        suite_result_end = _g_results + 18
        suite_first_test = Test1
        suite_first_mapped = ResetTimer6

        .segment "STARTUP"
suite_entry:
        sei
        ldx #$FF
        txs

        ; Clear all pinned zero-page scratch, the 18-byte result buffer, and
        ; wrapper state before entering the source tests.
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

        ; Expose Mikey and map the ROM/vector windows to RAM for the RTI vector.
        lda #$0C
        sta MAPCTL
        lda #<suite_default_irq
        sta INTVECTL
        lda #>suite_default_irq
        sta INTVECTH

        ; Observe the CPU's own bit-op behavior. Lynx I leaves the seeded
        ; zero-page byte unchanged; Lynx II executes RMB0 and clears it. The
        ; selected profile never supplies the value written to suite_model.
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

        ; Nonzero progress is guest-written before the untouched test entry.
        lda #$01
        sta suite_progress
        jsr _run_tests
        lda #$08
        sta suite_progress
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
