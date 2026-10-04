; Wildbits File Manager startup, GPL-3.0.
.setcpu "65C02"
.export __STARTUP__ : absolute = 1
.export _exit
.import _main, zerobss, initlib, donelib
.import _KernelBridge_Init, _KernelBridge_Restore
.importzp sp
.segment "STARTUP"
    lda #$ff
    sta sp
    lda #$9f
    sta sp+1
    jsr zerobss
    tsx
    stx saved_stack
    jsr _KernelBridge_Init
    jsr initlib
    jsr _main
_exit:
    jsr donelib
    jsr _KernelBridge_Restore
    ldx saved_stack
    txs
    rts
.segment "BSS"
saved_stack: .res 1
