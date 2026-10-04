; Wildbits File Manager MicroKernel bank bridge, GPL-3.0.
; The MicroKernel imports buffers and exports events through its RAM alias at
; $C000 in the USER LUT (kernel/calls.asm: import_data, next_event, read_data,
; read_ext). Directory records may occupy that window only between API calls.
; All foreground kernel calls use these wrappers. No application IRQ callback
; may call this bridge: enter/leave share one saved mapping.
.setcpu "65C02"
.import _Memory_GetMappedBankNum
.export _KernelBridge_Init, _KernelBridge_Restore
.segment "BSS"
kernel_bank: .res 1
initial_io: .res 1
saved_bank: .res 1
saved_io: .res 1
.segment "RODATA"
edit_controls: .byte $80, $91, $a2, $b3
.segment "CODE"

; Called at startup before any application mapping changes. Do not assume
; physical bank 6: retain the alias supplied by the running kernel/loader.
.proc _KernelBridge_Init
    lda $01
    sta initial_io
    lda #6
    jsr _Memory_GetMappedBankNum
    sta kernel_bank
    rts
.endproc

; Replace slot 6 in the ACTIVE LUT, preserving the complete MMU control byte,
; X, Y and flags. A returns the previous bank. No shared MMU scratch is used.
.proc swap_alias
    php
    sei
    phx
    phy
    tay
    lda $00
    pha
    and #3
    tax
    lda edit_controls,x
    sta $00
    ldx $0e
    sty $0e
    pla
    sta $00
    txa
    ply
    plx
    plp
    rts
.endproc

.proc enter_kernel
    php
    sei
    pha
    lda $01
    sta saved_io
    lda kernel_bank
    jsr swap_alias
    sta saved_bank
    pla
    plp
    rts
.endproc

.proc leave_kernel
    php                         ; retain kernel carry/status and A/X/Y results
    sei
    pha
    lda saved_bank
    jsr swap_alias
    lda saved_io
    sta $01
    pla
    plp
    rts
.endproc

.proc _KernelBridge_Restore
    lda kernel_bank
    jsr swap_alias
    lda initial_io
    sta $01
    rts
.endproc

.macro kernel_call name, vector
    .export name
    .proc name
        jsr enter_kernel
        jsr vector
        jmp leave_kernel
    .endproc
.endmacro

kernel_call _KernelCall_FF00, $FF00
kernel_call _KernelCall_FF04, $FF04
kernel_call _KernelCall_FF08, $FF08
kernel_call _KernelCall_FF0C, $FF0C
kernel_call _KernelCall_FF18, $FF18
kernel_call _KernelCall_FF44, $FF44
kernel_call _KernelCall_FF5C, $FF5C
kernel_call _KernelCall_FF60, $FF60
kernel_call _KernelCall_FF64, $FF64
kernel_call _KernelCall_FF68, $FF68
kernel_call _KernelCall_FF6C, $FF6C
kernel_call _KernelCall_FF70, $FF70
kernel_call _KernelCall_FF78, $FF78
kernel_call _KernelCall_FF7C, $FF7C
kernel_call _KernelCall_FF80, $FF80
kernel_call _KernelCall_FF84, $FF84
kernel_call _KernelCall_FF88, $FF88
kernel_call _KernelCall_FFF0, $FFF0
