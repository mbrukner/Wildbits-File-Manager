; Wildbits File Manager native viewer row rendering, GPL-3.0.
; Rows are assembled in RAM, then written once to character memory. Attributes
; are initialized on entry; page advances overwrite rows without clearing first.
.setcpu "65C02"
.importzp sp, ptr1, ptr2
.import incsp1, incsp5, _Keyboard_GetChar
.export _Viewer_BeginText, _Viewer_BeginHex, _Viewer_ClearScreen
.export _Viewer_TextRow, _Viewer_HexRow, _Viewer_WaitKey
.segment "OVERLAY_EM"
row_buffer: .res 80, $20
row_number: .byte 0
last_row: .byte 1
prepared: .byte 0
body_attr: .byte 0
text_count: .byte 0
text_next: .byte 0
hex_digits: .byte "0123456789ABCDEF"
row_low:
.repeat 60, row
    .byte <($c000 + row * 80)
.endrepeat
row_high:
.repeat 60, row
    .byte >($c000 + row * 80)
.endrepeat

; Preserve the caller's filename in A/X.
.proc _Viewer_BeginText
    pha
    lda #$f0
    bra begin
.endproc
.proc _Viewer_BeginHex
    pha
    lda #$b0
    ; fall through
.endproc
begin:
    sta body_attr
    stz prepared
    lda #1
    sta last_row
    pla
    rts

; Text_ClearScreen-compatible ABI: A=background, stack=foreground (discarded).
; Only the first page sets attributes and blanks the two heading rows.
.proc _Viewer_ClearScreen
    lda #1
    sta last_row
    lda prepared
    bne done
    inc prepared
    lda $01
    pha
    lda #3
    sta $01
    stz ptr2
    lda #$c0
    sta ptr2+1
    lda body_attr
    ldx #18
    ldy #0
 full_page:
    sta (ptr2),y
    iny
    bne full_page
    inc ptr2+1
    dex
    bne full_page
 tail:
    sta (ptr2),y
    iny
    cpy #192
    bne tail
    lda #2
    sta $01
    lda #$20
    ldy #159
 heading:
    sta $c000,y
    dey
    cpy #$ff
    bne heading
    pla
    sta $01
 done:
    jmp incsp1
.endproc

; Write all 80 cells. ptr1 remains intact for the text remainder calculation.
.proc write_row
    ldx row_number
    stx last_row
    lda row_low,x
    sta ptr2
    lda row_high,x
    sta ptr2+1
    lda $01
    pha
    lda #2
    sta $01
    ldy #79
 copy:
    lda row_buffer,y
    sta (ptr2),y
    dey
    bpl copy
    pla
    sta $01
    rts
.endproc

; Private one-row text ABI: stack = width (80), y, x (0), message low/high.
; A=max rows (1). Returns A/X = remainder or NULL; consumes five stack bytes.
; Preserve the existing word-wrap, CR, LF and CRLF rules without mutating input.
.proc _Viewer_TextRow
    ldy #3
    lda (sp),y
    sta ptr1
    iny
    lda (sp),y
    sta ptr1+1
    ldy #1
    lda (sp),y
    sta row_number
    ldy #0
 scan:
    lda (ptr1),y
    beq end_string
    cmp #13
    beq newline
    cmp #10
    beq newline
    cpy #80
    beq wrap
    iny
    bra scan
 newline:
    sty text_count
    iny
    cmp #13
    bne next
    lda (ptr1),y
    cmp #10
    bne next
    iny
    bra next
 wrap:
    lda (ptr1),y
    cmp #$20
    beq split
    dey
    bne wrap
    ldy #80
 split:
    sty text_count
    lda (ptr1),y
    cmp #$20
    bne next
    iny
    bra next
 end_string:
    sty text_count
 next:
    sty text_next
    ldy #0
 copy:
    cpy text_count
    beq pad
    lda (ptr1),y
    sta row_buffer,y
    iny
    bra copy
 pad:
    cpy #80
    beq draw
    lda #$20
    sta row_buffer,y
    iny
    bra pad
 draw:
    jsr write_row
    ldy text_next
    lda (ptr1),y
    beq no_remainder
    tya
    clc
    adc ptr1
    ldx ptr1+1
    bcc done
    inx
    bra done
 no_remainder:
    ldx #0
 done:
    jmp incsp5
.endproc

; A/X = 16-byte input pointer; stack = y, offset low/mid/high/unused.
; Consumes five bytes. Layout matches the original hex viewer exactly.
.proc _Viewer_HexRow
    sta ptr1
    stx ptr1+1
    lda (sp)
    sta row_number
    lda #$20
    ldx #79
 clear:
    sta row_buffer,x
    dex
    bpl clear
    lda #'$'
    sta row_buffer+1
    ldy #3
    lda (sp),y
    ldy #2
    jsr put_hex
    ldy #2
    lda (sp),y
    ldy #4
    jsr put_hex
    ldy #1
    lda (sp),y
    ldy #6
    jsr put_hex
    ldx #61
    ldy #11
 bytes:
    lda (ptr1)
    sta row_buffer,x
    jsr put_hex
    iny
    inc ptr1
    bne advance
    inc ptr1+1
 advance:
    inx
    cpx #77
    bne bytes
    jsr write_row
    jmp incsp5
.endproc

; A=byte, Y=output column; advances Y by two, preserves X.
.proc put_hex
    phx
    pha
    lsr
    lsr
    lsr
    lsr
    tax
    lda hex_digits,x
    sta row_buffer,y
    iny
    pla
    and #15
    tax
    lda hex_digits,x
    sta row_buffer,y
    iny
    plx
    rts
.endproc

; Erase only rows not overwritten by this page, including the bottom margin.
; This runs before waiting for input, so the final short page has no stale rows.
.proc _Viewer_WaitKey
    lda $01
    pha
    lda #2
    sta $01
    ldx last_row
    inx
 row:
    cpx #60
    bcs done
    lda row_low,x
    sta ptr2
    lda row_high,x
    sta ptr2+1
    lda #$20
    ldy #79
 clear:
    sta (ptr2),y
    dey
    bpl clear
    inx
    bra row
 done:
    pla
    sta $01
    jmp _Keyboard_GetChar
.endproc
