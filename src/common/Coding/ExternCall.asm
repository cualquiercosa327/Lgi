;============================================
; ExternCall.asm file
; Custom build rule:
;	cd $(IntDir)
;	"C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\x86_amd64\ml64.exe" $(InputPath) /c
; Output:
;	$(IntDir)\$(InputName).obj
;============================================
option casemap :none

.code


;--------------------------------------------
CallExtern64 PROC

	mov r12, rcx		; save FuncAddr
	mov R13, rdx		; save RetAddr
	mov r14d, r8d		; save Args
	mov r15, r9			; save ArgAddr

	; First arg
	cmp r14d, 0
	jle NoArg1
	mov rcx, [r15]
	add r15, 8
NoArg1:

	; Second arg
	cmp r14d, 1
	jle NoArg2
	mov rdx, [r15]
	add r15, 8
NoArg2:

	; Third arg
	cmp r14d, 2
	jle NoArg3
	mov r8, [r15]
	add r15, 8
NoArg3:

	; Forth arg
	cmp r14d, 3
	jle NoArg4
	mov r9, [r15]
	add r15, 8
NoArg4:

	; Other args?

	push rcx
	push rdx
	push r8
	push r9
	call r12			; Call the function
	pop r9
	pop r8
	pop rdx
	pop rcx
	mov [r13], rax		; Save the return value

	ret

CallExtern64 ENDP
;----------------------------------------------
END
;----------------------------------------------

; To compile this file use:
; ml.exe Addup.asm /c /coff