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


;extract and assemble filename as 8.3 from FCB 

		LD	HL,FCB+1
		LD	DE,filename
		ld	c,8
floop:		ld	a,(hl)
		ld	(de),a
		cp	a," "
		jr	z,getext
		inc	hl
		inc	de
		dec 	c
		ld	a,c
		cp	a,0
		jr	nz,floop

getext:		ld	a,"."
		ld	(de),a
		inc	de
		ld	hl,FCB+9
		ld	bc,3
		ldir

; Simply print the filename for diagnostic purposes
		LD	C,9
		LD	DE,filename
		CALL	BDOS
	

;Call ESP8266 to check for the file's existence and size.

		ld	bc,dparm
		ld	hl,BUFF
		out	(c),L
		inc	bc
		out	(c),H		;Write buffer address to command buffer
		inc	bc
		ld	hl, filename
lloop1		ld	a,(hl)
		cp	a,20h
		jr	z,eof
		out	(c),a		;Write filename to command buffer
		inc	hl
		inc	bc
		cp	0
		jr	nz,lloop1
		
eof:		xor	a
		out	(c),a		;Terminate filename with with 00h

		ld	bc,dcmd
		ld	a,4
		out	(c),a		;Send file open command
lloop1a		in	a,(c)
		jr	nz,lloop1a	;Wait for it to complete

; Simply print the block size for diagnostic purposes
		ld	de,decbuff
		ld	hl,(BUFF)
		ld	(blocks),hl	;Save block count

;		call	Num2Dec		
;		LD	C,9
;		LD	DE,decbuff
;		CALL	BDOS

; Check if file exists i.e Blocks >0
		ld	hl,(blocks)
		xor	a
		cp	a,h
		jr	nz,copy
		cp	a,l
		jr	z,finish

copy:
		ld	hl,0
		ld	(block),hl	;Clear current block number

;Delete file if it already exists
		LD	C,DELF
		LD	DE,FCB
		CALL	BDOS
;Make new file
		LD	C,MAKEF
		LD	DE,FCB
		CALL	BDOS

	
;Read Block
blocklp:	ld	bc,dparm
		ld	hl,BUFF
		out	(c),L
		inc	bc
		out	(c),H		;Write buffer address to command buffer
		inc	bc
		ld	hl,(block)
		out	(c),L
		inc	bc
		out	(c),H		;Write block number to command buffer

		ld	bc,dcmd
		ld	a,5
		out	(c),a		;Send file block read command
readwt		in	a,(c)
		jr	nz,readwt	;Wait for it to complete

;write block to disk
		LD	C,WRITES
		LD	DE,FCB
		CALL	BDOS

		ld	HL,(block)
		inc	HL
		ld	(block),HL	;Increment block number

		ld	HL,(blocks)
		dec	HL
		ld	(blocks),HL	;Decrement block counter
		xor	a
		cp	a,h
		jr	nz,blocklp
		cp	a,l
		jr	nz,blocklp

;Close file
		LD	C,CLOSEF
		LD	DE,FCB
		CALL	BDOS

finish:
;		JP	REBOOT
		ret

	
; Get a char into A
GETCHR: 	LD 	C,CONINP
		CALL 	BDOS
		RET


; Write A to output
PUTCHR: 	LD 	C,CONOUT
		LD 	E,A
		CALL	 BDOS
		RET


;------------------------------------------------------------------------------
; Convert 16 bit Binary in HL to ASCII Decimal buffer in DE
;------------------------------------------------------------------------------
Num2Dec		ld	bc,-10000
		call	Num1
		ld	bc,-1000
		call	Num1
		ld	bc,-100
		call	Num1
		ld	c,-10
		call	Num1
		ld	c,b

Num1		ld	a,'0'-1
Num2		inc	a
		add	hl,bc
		jr	c,Num2
		sbc	hl,bc
		ld	(de),a
		inc	de
		ret


byteCount 	.DB	0H

blocks		.dw	0
block		.dw	0
filename	.byte	"           "
		.byte	0,0dH,0ah,"$"
decbuff		.byte	"       Blocks"
		.byte	0,0dH,0ah,"$"


		.END
