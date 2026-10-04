; Wildbits File Manager native directory storage, GPL-3.0.
; One physical 8 KiB bank per pane, 256 slots of 32 bytes. The UI admits
; 255 entries. A slot contains the 18-byte file record and its 6-byte list node.
; No file or node allocation uses the resident heap. A mapped record is valid
; until a different pane is selected. Filename/string helpers save and restore
; slot 6; display helpers save and restore the I/O control register.
.setcpu "65C02"
.include "layout.inc"
.include "hardware.inc"
.importzp sp, _zp_bank_num
.import _Memory_SwapInNewBank, _App_SetFilenameInEM
.import _File_GetFileTypeFromExtension, pushax, pusha, incsp2, addysp
.export _Directory_Select, _Directory_SelectFolder, _Directory_SelectPanel
.export _Directory_RecordAddress, _File_New, _File_Destroy
.export _Folder_DestroyAllFiles, _List_NewItem, _List_Destroy, _List_AddItem

RECORD_BANK = DIRECTORY_RECORD_BANK
FOLDER_OWNER = WB2KFolderObject_panel_id_
PANEL_DISK = WB2KViewPanel_for_disk_
FILE_SIZE = WB2KFileObject
NODE_SIZE = WB2KList
.segment "ZEROPAGE"
map_ptr: .res 2
record_ptr: .res 2
name_ptr: .res 2
date_ptr: .res 2
head_ptr: .res 2
old_head: .res 2

.segment "CODE"
.proc _Directory_Select
    php
    sei
    phx
    phy
    and #1
    clc
    adc #RECORD_BANK
    sta _zp_bank_num
    lda #6
    jsr _Memory_SwapInNewBank
    lda #4
    sta $01
    ply
    plx
    plp
    rts
.endproc

.proc _Directory_SelectFolder
    sta map_ptr
    stx map_ptr+1
    ora map_ptr+1
    beq done
    ldy #FOLDER_OWNER
    lda (map_ptr),y
    jsr _Directory_Select
 done:
    rts
.endproc

.proc _Directory_SelectPanel
    sta map_ptr
    stx map_ptr+1
    ldy #PANEL_DISK
    lda (map_ptr),y
    beq done
    ldy #4
    lda (map_ptr),y
    jmp _Directory_Select
 done:
    rts
.endproc

; A = slot ID; returns A/X = $C000 + ID*32.
.proc _Directory_RecordAddress
    sta record_ptr
    lsr
    lsr
    lsr
    ora #$c0
    tax
    lda record_ptr
    asl
    asl
    asl
    asl
    asl
    sta record_ptr
    stx record_ptr+1
    rts
.endproc

; Bridge to the imported routines' existing ABI. Stack arguments, starting
; at (sp): row, type, size[4], directory, name[2], pane. A/X = DateTime*.
.proc _File_New
    sta date_ptr
    stx date_ptr+1
    ldy #9
    lda (sp),y
    jsr _Directory_Select
    lda (sp)
    jsr _Directory_RecordAddress
    lda #0
    ldy #31
 clear:
    sta (record_ptr),y
    dey
    bpl clear
    ldy #0
 size_loop:
    iny
    iny
    lda (sp),y
    dey
    dey
    sta (record_ptr),y
    iny
    cpy #4
    bne size_loop
    ldy #0
 date_loop:
    lda (date_ptr),y
    iny
    iny
    iny
    iny
    sta (record_ptr),y
    dey
    dey
    dey
    cpy #6
    bne date_loop
    ldy #6
    lda (sp),y
    ldy #10
    sta (record_ptr),y
    ldy #9
    lda (sp),y
    ldy #12
    sta (record_ptr),y
    lda (sp)
    iny
    sta (record_ptr),y
    ldy #16
    sta (record_ptr),y
    ldy #1
    lda (sp),y
    ldy #14
    sta (record_ptr),y
    ldy #7
    lda (sp),y
    sta name_ptr
    iny
    lda (sp),y
    sta name_ptr+1
    lda record_ptr
    ldx record_ptr+1
    jsr pushax
    lda name_ptr
    ldx name_ptr+1
    jsr _App_SetFilenameInEM
    ldy #14
    lda (record_ptr),y
    cmp #$10
    bne done
    jsr pusha
    lda name_ptr
    ldx name_ptr+1
    jsr _File_GetFileTypeFromExtension
    ldy #14
    sta (record_ptr),y
 done:
    lda record_ptr
    ldx record_ptr+1
    ldy #10
    jmp addysp
.endproc

.proc _File_Destroy
    sta map_ptr
    stx map_ptr+1
    lda #0
    sta (map_ptr)
    ldy #1
    sta (map_ptr),y
    rts
.endproc

.proc _Folder_DestroyAllFiles
    sta map_ptr
    stx map_ptr+1
    lda (map_ptr)
    sta head_ptr
    ldy #1
    lda (map_ptr),y
    sta head_ptr+1
    ora head_ptr
    beq count
    lda #0
    sta (head_ptr)
    sta (head_ptr),y
 count:
    lda #0
    ldy #6
    sta (map_ptr),y
    iny
    sta (map_ptr),y
    lda #$ff
    iny
    sta (map_ptr),y
    iny
    sta (map_ptr),y
    rts
.endproc

.proc _List_NewItem
    sta record_ptr
    stx record_ptr+1
    clc
    adc #FILE_SIZE
    sta map_ptr
    txa
    adc #0
    sta map_ptr+1
    lda #0
    ldy #3
 clear:
    sta (map_ptr),y
    dey
    bpl clear
    ldy #4
    lda record_ptr
    sta (map_ptr),y
    iny
    lda record_ptr+1
    sta (map_ptr),y
    lda map_ptr
    ldx map_ptr+1
    rts
.endproc

.proc _List_Destroy
    sta map_ptr
    stx map_ptr+1
    lda #0
    sta (map_ptr)
    ldy #1
    sta (map_ptr),y
    rts
.endproc

.proc _List_AddItem
    sta record_ptr
    stx record_ptr+1
    lda (sp)
    sta head_ptr
    ldy #1
    lda (sp),y
    sta head_ptr+1
    lda (head_ptr)
    sta old_head
    sta (record_ptr)
    lda (head_ptr),y
    sta old_head+1
    sta (record_ptr),y
    ora old_head
    beq set_head
    ldy #2
    lda record_ptr
    sta (old_head),y
    iny
    lda record_ptr+1
    sta (old_head),y
 set_head:
    lda #0
    ldy #2
    sta (record_ptr),y
    iny
    sta (record_ptr),y
    lda record_ptr
    sta (head_ptr)
    ldy #1
    lda record_ptr+1
    sta (head_ptr),y
    jmp incsp2
.endproc
