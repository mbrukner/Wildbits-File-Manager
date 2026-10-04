; Wildbits File Manager startup, GPL-3.0.
.setcpu "65C02"
.export __STARTUP__ : absolute = 1
.export _exit
.import _main, zerobss, initlib, donelib
.importzp sp
.segment "STARTUP"
    tsx
    stx saved_stack
    lda #$ff
    sta sp
    lda #$9f
    sta sp+1
    jsr zerobss
    jsr initlib
    jsr _main
_exit:
    jsr donelib
    ldx saved_stack
    txs
    rts
.segment "BSS"
saved_stack: .res 1
