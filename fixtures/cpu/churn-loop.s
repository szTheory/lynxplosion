; churn-loop: project-authored never-ending test program (Phase 2, plan 02-07).
; Load address $0200. 46 bytes. It reads seeded RAM, writes page 6, zero page
; and the stack, never touches the D flag, and uses only opcodes whose lynx1 and
; lynx2 rows are identical, so it runs unchanged under none, lynx1 and lynx2.
; Assembled by the pinned cc65: ca65 --cpu 65SC02 then ld65 -C raw-0200.cfg
; (scripts/check.sh cpu-fixtures step). This file is the source of truth for
; the bytes; fixtures/cpu/churn-loop.hex must equal the assembled output.
;
; Bytes (SHA-256 dcee7216a88ff2ffaf3e4a2794c3369676fc47493d710f26cd8bbadd19745b77):
;   a2 00 a0 08 a9 00 85 40 a9 05 85 41 bd 00 04 45 30 71 40 9d 00 06 2a 48
;   20 2b 02 68 85 30 e8 d0 eb e6 31 80 e7 ea ea ea ea ea ea 04 32 60
;
; Timing (datasheet cycle counts, 5 ticks per cycle, flat5, PLACEHOLDER, not
; timing evidence). Prelude LDX#0 2, LDY#8 2, LDA#0 2, STA zp 3, LDA#5 2,
; STA zp 3 = 14 cycles = 70 ticks. Loop head is $020C. Control flow depends only
; on X, so every iteration with X from 0 to 254 (BNE taken) costs
;   LDA abs,X 4 + EOR zp 3 + ADC (zp),Y 5 + STA abs,X 5 + ROL A 2 + PHA 3
;   + JSR 6 + (TSB zp 5 + RTS 6) + PLA 4 + STA zp 3 + INX 2 + BNE 3
;   = 51 cycles = 255 ticks.
; The pointer ($40),Y is $0500 + 8, so there is no page crossing; LDA $0400,X
; and STA $0600,X never cross (low byte $00).
;
; Hand check after 325 ticks (prelude 70 + one iteration 255): consumed 325,
; PC $020C, X=1, Y=8, A=0, S=$FD, P reads $34 (the last flag write is INX with
; X=1, so N=0 and Z=0; C stays 0), RAM $01FD=00 $01FC=02 $01FB=1A $0600=00
; $0041=05 $0040=00 (RAM policy zero, profile none). After 70 + 255*k ticks:
; PC $020C and X=k.
;
; Golden workload shape: seeded RAM, 4096 ticks = 15 full iterations
; (70 + 255*15 = 3895) plus 201 ticks of the 16th, ending mid-instruction.
; Chunked 512-tick runs sum to 4096 and equal the single call.
;
; Sensitivity: the operand 08 of LDY #$08 (second instruction, byte offset 3)
; is a data byte, not an opcode; the content-sensitivity flip targets it so the
; seeded byte read by ADC ($40),Y changes and the digest moves for the intended
; reason. Operand bytes such as the BRA offset e7 are data, not opcodes.
;
; Opcode bytes by offset: 00 a2, 02 a0, 04 a9, 06 85, 08 a9, 0a 85, 0c bd,
; 0f 45, 11 71, 13 9d, 16 2a, 17 48, 18 20, 1b 68, 1c 85, 1e e8, 1f d0, 21 e6,
; 23 80, 25..2a ea x6, 2b 04 (TSB), 2d 60. None is a $x7 or $xF opcode.

        .setcpu "65SC02"
        .segment "CODE"

start:  ldx #$00
        ldy #$08
        lda #$00
        sta $40
        lda #$05
        sta $41
loop:   lda $0400,x
        eor $30
        adc ($40),y
        sta $0600,x
        rol a
        pha
        jsr sub
        pla
        sta $30
        inx
        bne loop
        inc $31
        bra loop
        .byte $EA, $EA, $EA, $EA, $EA, $EA
sub:    tsb $32
        rts
