; Wildbits File Manager banked page transfers, GPL-3.0.
; The caller's buffer may be in the $A000 overlay that is temporarily replaced
; by the target bank. Stage each 256-byte transfer through the existing resident
; interbank page, and access the caller's buffer only with its overlay mapped.
.setcpu "65C02"
.importzp sp, ptr1, ptr2, _zp_bank_num
.import _Memory_SwapInNewBank, incsp4, __INTERBANKBUFFSTART__
.export _App_EMDataCopy
.segment "CODE"

; A = direction (1: to bank, 0: from bank).
; Software stack: page, base bank, caller buffer low/high. Consumes four bytes.
.proc _App_EMDataCopy
    pha
    ldy #2
    lda (sp),y
    sta ptr1
    iny
    lda (sp),y
    sta ptr1+1
    lda (sp)
    and #31
    ora #$a0
    sta ptr2+1
    stz ptr2
    lda (sp)
    lsr
    lsr
    lsr
    lsr
    lsr
    ldy #1
    clc
    adc (sp),y
    sta _zp_bank_num
    pla
    cmp #1
    bne from_bank

    ; Save the caller's bytes before replacing its overlay.
    ldy #0
 stage_source:
    lda (ptr1),y
    sta __INTERBANKBUFFSTART__,y
    iny
    bne stage_source
    lda #5
    jsr _Memory_SwapInNewBank
    pha
    ldy #0
 write_bank:
    lda __INTERBANKBUFFSTART__,y
    sta (ptr2),y
    iny
    bne write_bank
    pla
    jsr restore_overlay
    jmp incsp4

 from_bank:
    lda #5
    jsr _Memory_SwapInNewBank
    pha
    ldy #0
 read_bank:
    lda (ptr2),y
    sta __INTERBANKBUFFSTART__,y
    iny
    bne read_bank
    pla
    jsr restore_overlay
    ; The caller's destination is visible again.
    ldy #0
 copy_to_caller:
    lda __INTERBANKBUFFSTART__,y
    sta (ptr1),y
    iny
    bne copy_to_caller
    jmp incsp4

 restore_overlay:
    sta _zp_bank_num
    lda #5
    jmp _Memory_SwapInNewBank
.endproc
