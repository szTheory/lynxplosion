; sum-trap: project-authored terminating test program (Phase 2, plan 02-07).
; Load address $0200, RAM policy zero, profile none. 35 bytes.
; Assembled by the pinned cc65: ca65 --cpu 65SC02 then ld65 -C raw-0200.cfg
; (scripts/check.sh cpu-fixtures step). This file is the source of truth for
; the bytes; fixtures/cpu/sum-trap.hex must equal the assembled output.
;
; Bytes (SHA-256 6ae9bada8fb004f5756821c0e756267d2a2e44e6f68928fe27984906acc00f33):
;   a2 05 a9 00 86 10 18 65 10 ca d0 f8 8d 00 03 20 20 02 a2 ff bd 01 02 48
;   a9 00 68 07 ea ea ea ea e6 11 60
;
; The $07 at $021B is a bit-op slot: profile none stops there with UNDEFINED.
; Under lynx1 it is a different documented opcode, so this program is a
; profile none program (the four NOPs after it are never reached under none).
;
; Hand-derived expectations. They come from a step-by-step derivation with the
; datasheet cycle counts, NOT from running the CPU. Cycle counts: SST rockwell
; templates and WDC/Rockwell datasheets agree for each instruction (LDX/LDA
; immediate 2, STX zp 3, CLC 2, ADC zp 3, DEX 2, BNE 3 taken / 2 not taken,
; STA abs 4, JSR 6, INC zp 5, RTS 6, LDA abs,X 5 with the page crossing,
; PHA 3, PLA 4). Ticks = 5 per cycle (flat5, PLACEHOLDER, not timing evidence).
;
;  PC    Instruction            Cyc  Cumulative cycles = ticks  Result
;  0200  LDX #$05                2    2 = 10                    X=05
;  0202  LDA #$00                2    4 = 20                    A=00
;  0204  STX $10                 3    7 = 35                    [10]=05
;  0206  CLC                     2    9 = 45                    C=0
;  0207  ADC $10                 3   12 = 60                    A=05
;  0209  DEX                     2   14 = 70                    X=04
;  020A  BNE $0204 (taken)       3   17 = 85
;        iteration 2 (X=4)      13   30 = 150                  A=09 X=03 [10]=04
;        iteration 3 (X=3)      13   43 = 215                  A=0C X=02 [10]=03
;        iteration 4 (X=2)      13   56 = 280                  A=0E X=01 [10]=02
;        iteration 5 (X=1)      12   68 = 340                  A=0F X=00 [10]=01 PC=020C
;              (STX 3, CLC 2, ADC 3, DEX 2, BNE not taken 2 = 12)
;  020C  STA $0300               4   72 = 360                  [0300]=0F
;  020F  JSR $0220               6   78 = 390                  push 02 at $01FD and 11 at $01FC, S=FB
;  0220  INC $11                 5   83 = 415                  [11]=01
;  0222  RTS                     6   89 = 445                  S=FD PC=0212
;  0212  LDX #$FF                2   91 = 455                  X=FF
;  0214  LDA $0201,X             5   96 = 480                  EA=$0300 (carry from $01+$FF) A=0F
;  0217  PHA                     3   99 = 495                  [01FD]=0F (overwrites the 02) S=FC
;  0218  LDA #$00                2  101 = 505                  A=00
;  021A  PLA                     4  105 = 525                  A=0F S=FD
;  021B  $07                    stop                           UNDEFINED, consumed total 525, PC=021B
;
; Expected final state under profile none with a budget of at least 525:
;   consumed 525, stop undefined, PC $021B, A $0F, X $FF, Y $00, S $FD,
;   P reads $34 (N=0 Z=0 C=0 V=0 D=0 I=1; the declared power-on values,
;   S=$FD and P=$34 are unverified).
;   RAM: $0010=01 $0011=01 $0300=0F $01FC=11 $01FD=0F, everything else zero
;   except the loaded image.
; A second advance returns UNDEFINED with consumed 0 and an identical buffer.
;
; S2 edge expectations on this program:
;   advance 524: all 105 cycles are granted (the last at tick 520), consumed
;     exactly 524, stop BUDGET, registers already final (one cycle is still in
;     flight until tick 525). A following advance 1 consumes 1 tick (BUDGET),
;     the next call reports UNDEFINED with consumed 0.
;   advance 520: stops before the last PLA cycle: A is still 0 and S still $FC,
;     PC is already $021B.

        .setcpu "65SC02"
        .segment "CODE"

start:  ldx #$05
        lda #$00
loop:   stx $10
        clc
        adc $10
        dex
        bne loop
        sta $0300
        jsr sub
        ldx #$FF
        lda $0201,x
        pha
        lda #$00
        pla
        .byte $07
        .byte $EA, $EA, $EA, $EA
sub:    inc $11
        rts
