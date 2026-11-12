; Project-authored returning wrapper around the unchanged pinned audio/tests.s.
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
        .export suite_probe_result, suite_test_return, suite_after_tests
        suite_test_entry = _run_tests
        ; Pinned _run_tests is SEI, six test JSRs, ResetChannels JSR, CLI, RTS.
        ; The pinned-source hash check makes this source-derived boundary fail closed.
        suite_test_return = _run_tests + 23
        suite_result_end = _g_results + 18
        suite_first_test = Test1
        suite_first_mapped = ResetChannels

        .segment "STARTUP"
suite_entry:
        sei
        ldx #$FF
        txs

        ; Clear all 256 bytes of zero page, the result extent and wrapper state.
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

        ; Observe the CPU's own bit-op behavior, independent of the selected
        ; host profile. Lynx I leaves this seeded byte set; Lynx II clears it.
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
        ; The source suite ends with CLI while a timer IRQ can remain pending.
        ; Mask only the IRQ frame returning to the post-suite sentinel.
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
suite_progress: .res 1
suite_model:    .res 1
suite_done:     .res 1

        .segment "CODE"
suite_end:
