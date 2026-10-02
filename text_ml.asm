;
; text_ml.asm
;
;  Created on: Dec 16, 2024 (adapted from B128's text_ml.asm)
;      Author: micahbly
;

;
; "boilerplate"(??) cc65 stuff I copied from a compiled file
;
	.fopt	compiler,"cc65 v 2.18 - N/A"
	.setcpu	"65C02"
	.smart	on
	.autoimport	on
	.case	on
	.debuginfo	off
	.importzp	sp, sreg, regsave, regbank
	.importzp	tmp1, tmp2, tmp3, tmp4, ptr1, ptr2, ptr3, ptr4
	.importzp	_zp_old_io_page, _zp_old_bank_num, _zp_bank_slot, _zp_bank_num, _zp_temp_1
	.macpack	longbranch

; Function to export
	.export _Text_SetChar
;	.export _Text_Int2Hex
;	.export _Text_DrawString
;	.export _Text_FillBox
	.export	_Text_Invert
	.export _Text_DrawChars
;	.export _Text_DrawString
;	.export _Text_ScrollTextRowsDown
;	.export _Text_ScrollTextRowsUp
	.export _Text_SetMemLocForXY
	.export _Text_ScrollTextUp
	.export _Text_ScrollTextDown
	.export _Text_DrawByteAsHexChars

;	.export _text_memory_iterate
;	.export _text_setchar
;	.export _text_do_cr


; Variables to export
	.exportzp	_zp_ptr
	.exportzp	_zp_vram_ptr
	.exportzp	_zp_x
	.exportzp	_zp_y
	.exportzp	_zp_char		; if writing a char, the value to write
	.exportzp	_zp_attr		; if writing an attr, the value to write
	.exportzp	_zp_for_attr	; if 0, action will be towards char memory.

; maybe export. review later.
	.exportzp	_zp_sava
	.exportzp	_zp_savx
	.exportzp	_zp_savy
	.exportzp	_zp_x_cnt
	.exportzp	_zp_y_cnt
	.exportzp	_zp_func_ptr

; definitions - common

; Wildbits/k classic memory layout MMU
MMU_MEM_CTRL		= $0000	;// bit 0-1: activate LUT (exit editing); 4-5 LUT to be edited; 7: activate edit mode
MMU_IO_CTRL			= $0001	;// bits 0-1: IO page; bit 2: disable IO

; Tiny VICKY I/O pages
VICKY_IO_PAGE_REGISTERS			= 0	; Low level I/O registers
VICKY_IO_PAGE_FONT_AND_LUTS		= 1	; Text display font memory and graphics color LUTs
VICKY_IO_PAGE_CHAR_MEM			= 2	; Text display character matrix
VICKY_IO_PAGE_ATTR_MEM			= 3	; Text display color matrix

; bank slot numbers
BANK_IO				= $06	;// 0xc000-0xdfff
BANK_KERNAL			= $07	;// 0xe000-0xffff

; definitions - this class only

VRAM				= $C000	; WILDBITS classic memory schemes have both attr and chars at this address. use vicky page to control what gets written to.
VICKY_TEXT_X_POS	= $D014	; 2-byte
VICKY_TEXT_Y_POS	= $D016	; 2-byte

SCREEN_NUM_COLS		= 80
SCREEN_NUM_ROWS		= 60
SCREEN_LAST_COL		= 79
SCREEN_LAST_ROW		= 59

; variable reservations

.segment "ZEROPAGE" : zeropage

; -- ZEROPAGE starts at $10

;_zp_bank_slot:			.res 1	; $10
;_zp_bank_num:			.res 1
;_zp_old_bank_num:		.res 1
;_zp_to_addr:			.res 3
;_zp_from_addr:			.res 3
;_zp_copy_len:			.res 3
;_zp_phys_addr_lo:		.res 1
;_zp_phys_addr_med:		.res 1
;_zp_phys_addr_hi:		.res 1
;_zp_cpu_addr_lo:		.res 1
;_zp_cpu_addr_hi:		.res 1	; $20
;_zp_search_loc_byte:	.res 1
;_zp_search_loc_page:	.res 1
;_zp_search_loc_bank:	.res 1
;_zp_temp_1:				.res 1
;_zp_other_byte:			.res 1
;_zp_old_io_page:		.res 1	;-- $26

_zp_sava:			.res 1,$00
_zp_savx:			.res 1,$00
_zp_savy:			.res 1,$00

_zp_vram_ptr:		.res 2,$00
_zp_x:				.res 1,$00
_zp_y:				.res 1,$00
_zp_x_cnt:			.res 1,$00	; num columns to operate on
_zp_y_cnt:			.res 1,$00	; num rows to operate on
_zp_char:			.res 1,$00	; character to be displayed on screen, or character extracted from screen ram
_zp_attr:			.res 1,$00	; if processing an attr, the value to write or read
_zp_for_attr:		.res 1,$00	; if 0, action will be towards char memory.

_zp_func_ptr:		.res 2,$0000	; address of the function to be called by iterator functions
_zp_ptr:			.res 2,$0000
_zp_from_ptr:		.res 2,$0000
_zp_to_ptr:			.res 2,$0000
_zp_len:			.res 2,$00	; the length of operation requested. public.
_zp_cnt:			.res 2,$00	; for counting up towards zp_len. private.

; data

;.segment	"RODATA"

; nothing yet



.segment	"CODE"

; ML-only functions called by the public ones
;   Do NOT call these from C!
; ------------------


; routine that will start at a specified address, and iterate through a specified number of bytes,
; update the current address as it goes.
; it will jsr to the function whose address is found in zp_func_ptr on every loop
; will work on both the current execution bank, and on the indirection data bank (if $01 is set)
; does not rely on A, X, or Y being in any particular state after

_text_memory_iterate:
    lda _zp_len
    ora _zp_len+1
    beq @done
    lda _zp_func_ptr
    sta @loop+1
    lda _zp_func_ptr+1
    sta @loop+2
@loop:
    jsr $0000
    inc _zp_ptr
    bne @decrement
    inc _zp_ptr+1
@decrement:
    lda _zp_len
    bne @low
    dec _zp_len+1
@low:
    dec _zp_len
    lda _zp_len
    ora _zp_len+1
    bne @loop
@done:
    rts

; private part of invert function
invert_function:
	lda (_zp_ptr),y
	;back nibble
	and #$f0
	lsr a
	lsr a
	lsr a
	lsr a
	sta _zp_temp_1				; stash back nibble
	lda (_zp_ptr),y
	; fore nibble
	and #$0f
	asl a
	asl a
	asl a
	asl a
	ora _zp_temp_1
	sta (_zp_ptr),y
	rts



; asm call only function that won't get the extra cc65 jazz.
; is called by Text_SetChar, but other ASM routines can also use it.
;   calling function is responsible for making sure everything is set up
;     that includes saving .Y and .A if important.

_text_setchar:
    sta (_zp_vram_ptr),y
    ; Saturate at the last screen cell, otherwise advance with line wrap.
    lda _zp_x
    cmp #SCREEN_LAST_COL
    bcc @advance
    lda _zp_y
    cmp #SCREEN_LAST_ROW
    bcs @done
    stz _zp_x
    inc _zp_y
    bra @pointer
@advance:
    inc _zp_x
@pointer:
    inc _zp_vram_ptr
    bne @done
    inc _zp_vram_ptr+1
@done:
    rts

; asm-only call to update the VICKY's x and y cursor pos with latest from Text engine
; mangles .a. changes IO control.

_text_update_cur_pos:
	lda #VICKY_IO_PAGE_REGISTERS
	sta MMU_IO_CTRL
	lda _zp_x
	sta VICKY_TEXT_X_POS
	lda _zp_y
	sta VICKY_TEXT_Y_POS
	rts




; ---------------------------------------------------------------
; void __fastcall__ Text_Invert(int the_length)
; ---------------------------------------------------------------
;// Inverts >the_length< bytes of data in >buffer<
;// call Text_SetXY() prior to calling
;// fastcall should put length of run (2 bytes) into the A and X registers already.


.segment	"CODE"

.proc	_Text_Invert: near

.segment	"CODE"

	; calling method should already have caused zp_vram_ptr to be set. We copy that to zp_ptr
Text_Invert:
	sta _zp_len
	stx _zp_len+1
	lda _zp_vram_ptr
	sta _zp_ptr
	lda _zp_vram_ptr+1
	sta _zp_ptr+1

    lda MMU_IO_CTRL
    pha
	lda #VICKY_IO_PAGE_ATTR_MEM
	sta MMU_IO_CTRL

	ldy #$00		; set up for indirect indexing

	lda #<invert_function
	sta _zp_func_ptr
	lda #>invert_function
	sta _zp_func_ptr+1

	jsr _text_memory_iterate
    pla
    sta MMU_IO_CTRL
	rts

.endproc






; ---------------------------------------------------------------
; void __fastcall__ Text_SetChar(uint8_t the_char)
; ---------------------------------------------------------------
;//! Set a char at the current X/Y position, and advance cursor position by 1
;//! call Text_SetXY() prior to calling
;//! @param	the_char - the character to be used


.segment	"CODE"

.proc	_Text_SetChar: near

.segment	"CODE"

	; calling method should already have set zp_x, zp_y, and zp_vram_ptr

    phy
    pha
    lda MMU_IO_CTRL
    sta _zp_sava
    pla
    ldy _zp_sava
    phy
	ldy #VICKY_IO_PAGE_CHAR_MEM
	sty MMU_IO_CTRL

	ldy #$00		; set up for indirect indexing
	jsr _text_setchar
	jsr _text_update_cur_pos

    pla
    sta MMU_IO_CTRL
    ply
	rts

.endproc






; ---------------------------------------------------------------
; void __fastcall__ Text_DrawChars(int the_length)
; ---------------------------------------------------------------
;//! Copy n-bytes into display memory, at the current X/Y position
;//! set zp_ptr to the buffer address prior to calling
;//! call Text_SetXY() prior to calling
;//! @param	the_length - the number of characters to be drawn


.segment	"CODE"

.proc	_Text_DrawChars: near

.segment	"CODE"

	; calling method should already have set zp_ptr and zp_len and zp_vram_ptr

Draw_Chars:
    sta _zp_len
    stx _zp_len+1
    lda MMU_IO_CTRL
    pha
    lda #VICKY_IO_PAGE_CHAR_MEM
    sta MMU_IO_CTRL
    ldy #0
    lda _zp_len
    ora _zp_len+1
    beq @done
@loop:
    lda (_zp_ptr),y
    sta (_zp_vram_ptr),y
    inc _zp_ptr
    bne @dest
    inc _zp_ptr+1
@dest:
    inc _zp_vram_ptr
    bne @decrement
    inc _zp_vram_ptr+1
@decrement:
    lda _zp_len
    bne @low
    dec _zp_len+1
@low:
    dec _zp_len
    lda _zp_len
    ora _zp_len+1
    bne @loop
@done:
    pla
    sta MMU_IO_CTRL
    rts

.endproc




; ---------------------------------------------------------------
; void __fastcall__ _Text_SetMemLocForXY(void)
; ---------------------------------------------------------------
;//! Calculate the VRAM location of the specified coordinate, updates VICKY cursor pos
;//!  Set zp_x, zp_y before calling


.segment	"CODE"

.proc _Text_SetMemLocForXY: near

.segment	"CODE"

set_vram_address:
    lda MMU_IO_CTRL
    pha

	lda #>VRAM	; hi byte of $d000 VRAM base address
	sta _zp_vram_ptr+1
	lda #$00
	sta _zp_vram_ptr	; zp vram addr now reset to base $d000

	ldx _zp_vram_ptr+1	; #>VRAM

; calculate screen row

	ldy _zp_y
	iny         ; add one as preparation for loop
calcy:
	clc         ; clear carry for addition(s)
ccy0:
	dey         ; another row ?
	beq calcx   ; b: no
	adc #80     ; offset to next row
	bcc ccy0    ; no overflow
	inx         ; update high byte of address
	bcs calcy   ; b: forced (and clear carry again)

; add screen column

calcx:
	adc _zp_x		; carry is clear at this point
	bcc calc_done
	inx

calc_done:
	sta _zp_vram_ptr	; save pointer to screen location (LO)
	stx _zp_vram_ptr+1	; save pointer to screen location (HI)

	jsr _text_update_cur_pos
    pla
    sta MMU_IO_CTRL
	rts

.endproc




; ---------------------------------------------------------------
; void __near__ Text_ScrollTextUp (uint8_t num_cols)
; ---------------------------------------------------------------
;//! scrolls a horizontal portion of the text memory up ONE row.
;//!   e.g, row 0 is lost. row 1 becomes row 0, row 24 becomes row 23.
;//!   call Text_SetXY() first to establish the starting x,y
;//!   set _zp_y_cnt to the number of rows from starting y to include in the scroll
;//! @param	num_cols - the number of characters from starting x to include in the scroll


.segment	"CODE"

.proc	_Text_ScrollTextUp: near

.segment	"CODE"

	; requires that zp_x, zp_y, zp_vram_ptr, and zp_y_cnt have all been set prior to calling

	; LOGIC:
	; _zp_from_ptr will be the row below
	; _zp_to_ptr will be the row above
	; both are VRAM locs, so indirection bank will be set to 15 and left there
	; after one row's worth is copied, adjust subtract 80 from zp_ptr and loop back

	; common checks - same for scroll up or down
	; validate inputs
	sta _zp_x_cnt
    lda MMU_IO_CTRL
    pha
    lda _zp_x_cnt
	cmp #$00		; check that at least 1 char is being scrolled
	jeq @error
	cmp #(SCREEN_NUM_COLS+1)	; check that # cols is not greater than # of total cols on screen
	jcs @error

	lda _zp_y_cnt
	cmp #$00		; check that text is being scrolled at least 1 row
	jeq @error
	cmp #SCREEN_NUM_ROWS	; check that # rows is not greater than # of total cols on screen - 1
	jcs @error

	lda _zp_x
    clc
    adc _zp_x_cnt
    cmp #(SCREEN_NUM_COLS+1)
    jcs @error

	lda _zp_y
	cmp #$00		; check that starting y is not first row: we are scrolling up, and so starting at 0 makes no sense
	jeq @error
	cmp #(SCREEN_LAST_ROW+1)	; check that starting y is not past the last row
	jeq @error

    clc
    adc _zp_y_cnt
    cmp #(SCREEN_NUM_ROWS+1)
    jcs @error

	ldy #$00		; set up for indirect indexing
	lda #VICKY_IO_PAGE_CHAR_MEM
	sta MMU_IO_CTRL

	; set up _zp_from_ptr and _zp_to_ptr from zp_vram_ptr
	lda _zp_vram_ptr+1
	sta _zp_from_ptr+1
	sta _zp_to_ptr+1
	lda _zp_vram_ptr
	sta _zp_from_ptr
	sta _zp_to_ptr
	; set _zp_to_ptr one row above _zp_from_ptr
	sec
	sbc #SCREEN_NUM_COLS
	sta _zp_to_ptr
	bcs @loop
	dec _zp_to_ptr+1

@loop:
	lda (_zp_from_ptr),y
	sta (_zp_to_ptr),y
	iny
	cpy _zp_x_cnt
	bne @loop

@row_done:
	dec _zp_y_cnt
	lda _zp_y_cnt
	cmp #$00
	beq @done

	ldy #$00	; reset Y to start again at zp_vram_ptr and zp_ptr

	; add one row from both to and from
	; for to ptr, can just use from ptr loc as it is already what we need to write to
	lda _zp_from_ptr+1
	sta _zp_to_ptr+1
	lda _zp_from_ptr
	sta _zp_to_ptr

@add_one_row:
	clc
	adc #SCREEN_NUM_COLS
	sta _zp_from_ptr
	lda _zp_from_ptr+1
	adc #$00
	sta _zp_from_ptr+1
	jmp @loop

@error:
@done:
    pla
    sta MMU_IO_CTRL
	rts

.endproc




; ---------------------------------------------------------------
; void __near__ Text_ScrollTextDown (uint8_t num_cols)
; ---------------------------------------------------------------
;//! scrolls a horizontal portion of the text memory down ONE row.
;//!   e.g, row 24 is lost. row 23 becomes row 24, row 1 becomes row 0
;//!   call Text_SetXY() first to establish the starting x,y
;//!   set _zp_y_cnt to the number of rows from starting y to include in the scroll
;//! @param	num_cols - the number of characters from starting x to include in the scroll

.segment	"CODE"

.proc	_Text_ScrollTextDown: near

.segment	"CODE"

	; requires that zp_x, zp_y, zp_vram_ptr, and zp_y_cnt have all been set prior to calling

	; LOGIC:
	; _zp_from_ptr will be the row above
	; _zp_to_ptr will be the row below
	; both are VRAM locs, so indirection bank will be set to 15 and left there
	; after one row's worth is copied, adjust subtract 80 from zp_ptr and loop back

	; common checks - same for scroll up or down
	; validate inputs
	sta _zp_x_cnt
    lda MMU_IO_CTRL
    pha
    lda _zp_x_cnt
	cmp #$00		; check that at least 1 char is being scrolled
	jeq @error
	cmp #(SCREEN_NUM_COLS+1)	; check that # cols is not greater than # of total cols on screen
	jcs @error

	lda _zp_y_cnt
	cmp #$00		; check that text is being scrolled at least 1 row
	jeq @error
	cmp #SCREEN_NUM_ROWS	; check that # rows is not greater than # of total cols on screen - 1
	jcs @error

	lda _zp_x
    clc
    adc _zp_x_cnt
    cmp #(SCREEN_NUM_COLS+1)
    jcs @error

	lda _zp_y
    jeq @error
    cmp #SCREEN_NUM_ROWS
    jcs @error

    cmp _zp_y_cnt
    jcc @error

	ldy #$00		; set up for indirect indexing
	lda #VICKY_IO_PAGE_CHAR_MEM
	sta MMU_IO_CTRL

	; set up _zp_from_ptr and _zp_to_ptr from zp_vram_ptr
	lda _zp_vram_ptr+1
	sta _zp_from_ptr+1
	sta _zp_to_ptr+1
	lda _zp_vram_ptr
	sta _zp_from_ptr
	sta _zp_to_ptr
	; set _zp_from_ptr one row above _zp_to_ptr
@subtract_one_row:
	sec
	sbc #SCREEN_NUM_COLS
	sta _zp_from_ptr
	bcs @loop
	dec _zp_from_ptr+1

@loop:
	lda (_zp_from_ptr),y
	sta (_zp_to_ptr),y
	iny
	cpy _zp_x_cnt
	bne @loop

@row_done:
	dec _zp_y_cnt
	lda _zp_y_cnt
	cmp #$00
	beq @done

	ldy #$00	; reset Y to start again at zp_vram_ptr and zp_ptr

	; subtract one row from both to and from
	; for to ptr, can just use from ptr loc as it is already what we need to write to
	lda _zp_from_ptr+1
	sta _zp_to_ptr+1
	lda _zp_from_ptr
	sta _zp_to_ptr

	jmp @subtract_one_row

@error:
@done:
    pla
    sta MMU_IO_CTRL
	rts

.endproc











; ---------------------------------------------------------------
; void __fastcall__ Text_DrawByteAsHexChars (uint8_t the_byte)
; ---------------------------------------------------------------
;//! draws the byte passed as 2 hex characters, at the current X,y pos
;//!   call Text_SetXY() first to establish the starting x,y
;//! @param	the_byte - the 8-bit number to be converted to hex and written to screen as 2 hex chars

.segment	"CODE"

.proc	_Text_DrawByteAsHexChars: near

.segment	"CODE"

	; requires that zp_x, zp_y, zp_vram_ptr have all been set prior to calling

	; LOGIC:
	;   Based on code from Jim Butterfield's SuperMon64, as found here:
	;   https://github.com/jblang/supermon64/blob/master/supermon64.asm

DrawByteAsHexChars:
	jsr @convert2hex	; a has first hex digit (upper), x has second (lower) after this
	jsr _Text_SetChar
	txa
	jsr _Text_SetChar
	rts

	; convert byte in A to hex digits
@convert2hex:
	pha				; save byte
	jsr @ascii		; do low nibble
	tax				; save first converted hex char in X
	pla				; get full byte back
	lsr a			; do upper nibble
	lsr a
	lsr a
	lsr a
@ascii:
	and #$0f		; clear upper nibble
	cmp #$0a		; if less than 10, don't need next step
	bcc @ascii1
	adc #$06		; skip ascii chars between '9' and 'A'
@ascii1:
	adc #$30		; add 48 to get to 0-9 or A-F

	rts

.endproc


