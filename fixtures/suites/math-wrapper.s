; Project-authored returning wrapper around unchanged pinned math/tests.s.
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
        ; The pinned runner is SEI, eight test JSRs, CLI, RTS.
        ; Its source hash is checked before/after every build.
        suite_test_return = _run_tests + 26
        suite_result_end = _g_results + 18
        suite_first_test = Test1
        suite_first_mapped = Test1

        .segment "STARTUP"
suite_entry:
        sei
        ldx #$FF
        txs

        ; Test8's source WaitVBlank polls Timer2. Configure the linked
        ; Timer0/2 chain from the test's declared 60 Hz period.
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

        ; Clear guest scratch, all 18 source result bytes and wrapper state.
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
        ; Mask an IRQ only on the saved return frame after _run_tests.
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
