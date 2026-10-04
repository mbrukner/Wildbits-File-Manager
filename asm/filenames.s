; Wildbits File Manager banked filename access, GPL-3.0.
.setcpu "65C02"
.include "hardware.inc"
.importzp _zp_bank_num, sp
.import _Memory_SwapInNewBank, _global_retrieved_em_filename, incsp2
.export _App_GetFilenameFromEM, _App_SetFilenameInEM
.segment "ZEROPAGE"
file_ptr: .res 2
slot_ptr: .res 2
text_ptr: .res 2
.segment "CODE"
; A/X = record pointer. Cache its fields before mapping its filename bank.
.proc address
    sta file_ptr
    stx file_ptr+1
    ldy #12
    lda (file_ptr),y
    clc
    adc #FILENAME_BANK
    sta _zp_bank_num
    iny
    lda (file_ptr),y
    tax
    lsr
    lsr
    lsr
    ora #$c0
    sta slot_ptr+1
    txa
    asl
    asl
    asl
    asl
    asl
    sta slot_ptr
    rts
.endproc
.proc _App_GetFilenameFromEM
    php
    sei
    jsr address
    lda $01
    pha
    lda #4
    sta $01
    lda #6
    jsr _Memory_SwapInNewBank
    pha
    lda _global_retrieved_em_filename
    sta text_ptr
    lda _global_retrieved_em_filename+1
    sta text_ptr+1
    ldy #31
 copy:
    lda (slot_ptr),y
    sta (text_ptr),y
    dey
    bpl copy
    lda #0
    ldy #31
    sta (text_ptr),y
    pla
    sta _zp_bank_num
    lda #6
    jsr _Memory_SwapInNewBank
    pla
    sta $01
    plp
    lda text_ptr
    ldx text_ptr+1
    rts
.endproc
.proc _App_SetFilenameInEM
    sta text_ptr
    stx text_ptr+1
    php
    sei
    ldy #1
    lda (sp),y
    tax
    lda (sp)
    jsr address
    lda $01
    pha
    lda #4
    sta $01
    lda #6
    jsr _Memory_SwapInNewBank
    pha
    ldy #0
 copy:
    lda (text_ptr),y
    beq terminate
    sta (slot_ptr),y
    iny
    cpy #31
    bne copy
 terminate:
    lda #0
    sta (slot_ptr),y
    pla
    sta _zp_bank_num
    lda #6
    jsr _Memory_SwapInNewBank
    pla
    sta $01
    plp
    jmp incsp2
.endproc
