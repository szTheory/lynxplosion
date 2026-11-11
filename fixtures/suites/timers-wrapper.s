; Original wrapper around the unchanged pinned timers/tests.s suite.
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
        .export suite_test_return, suite_after_tests
        suite_test_entry = _run_tests
        ; The pinned _run_tests epilogue is 7 JSR tests + ResetTimers + CLI + RTS.
        ; The pin/hash check makes this source-derived gate fail closed if it moves.
        suite_test_return = _run_tests + 26
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

        ; Detect the executing CPU from its RMB0 behavior, independently of
        ; the selected host profile. Lynx I leaves the seeded byte set; Lynx
        ; II executes RMB0 and clears it.
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

        lda #$0C
        sta MAPCTL
        lda #<suite_default_irq
        sta INTVECTL
        lda #>suite_default_irq
        sta INTVECTH

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

suite_default_irq:
        ; The pinned suite ends with CLI while Test7's timer IRQ remains pending.
        ; Preserve its results and pending latch, but mask only the IRQ frame
        ; whose return PC is the caller continuation after _run_tests returned.
        ; An unexpected interrupt during tests keeps the original status.
        pha
        txa
        pha
        tsx
        lda $0104,x               ; saved return PC low byte
        cmp #<suite_after_tests
        bne @irq_done
        lda $0105,x               ; saved return PC high byte
        cmp #>suite_after_tests
        bne @irq_done
        lda $0103,x               ; saved status after hardware interrupt entry
        ora #$04
        sta $0103,x
@irq_done:
        pla
        tax
        pla
        rti

        .segment "BSS"
suite_progress: .res 1
suite_model:    .res 1
suite_done:     .res 1

        .segment "CODE"
suite_end:
