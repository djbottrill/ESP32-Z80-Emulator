;==================================================================================
; Contents of this file are copyright David Bottrill
; 
;
; You have permission to use this for NON COMMERCIAL USE ONLY
; If you wish to use it elsewhere, please include an acknowledgement to myself.
;
;
;==================================================================================

TPA		.EQU	100H
REBOOT		.EQU	0H
BDOS		.EQU	5H
CONIO		.EQU	6
CONINP		.EQU	1
CONOUT		.EQU	2
PSTRING		.EQU	9
MAKEF		.EQU	22
CLOSEF		.EQU	16
WRITES		.EQU	21
DELF		.EQU	19
SETUSR		.EQU	32

CR		.EQU	0DH
LF		.EQU	0AH

FCB		.EQU	05CH
BUFF		.EQU	080H

;Virtual I/O Registers
uart_a_tx	equ	$ff00
uart_a_rx	equ	$ff10
uart_b_tx	equ	$ff01
uart_b_rx	equ	$ff11
dparm		equ	$ff30
dcmd		equ	$ff20


		.ORG TPA



	

;Call ESP8266 to set the directory path on the SD Card

		ld	bc,dparm
		ld	hl,BUFF
		out	(c),L
		inc	bc
		out	(c),H		;Write buffer address to command buffer
		ld	bc,dcmd
		ld	a,7
		out	(c),a		;Send set path command
lloop1a		in	a,(c)
		jr	nz,lloop1a	;Wait for it to complete
		ret


		.END
