
;--- JLoad's PE file loader
;--- this code is copied to linear address ?SYSTEMADDR when
;--- JLoad is running for the first time.
;--- best viewed with TAB size 4

	.386
	.model flat

	option casemap:none
	option proc:private

	include jemm.inc
	include jemm32.inc
	include jlm.inc
	include jload.inc
	include jload32.inc
	include winnt.inc
	include debug.inc

DATA_	segment dword public 'CODE'

;--- variables

g_dwIDT		dd 0	;linear address IDT

if ?USEVCPIEXIT
vcpi		label fword
vcpiofs		dd 0
vcpisel 	dd 0
endif

DATA_	ends

	.code

;--- entry from real-mode. This must be at offset 0.
;--- CS,DS,ES=Flat, SS unknown
;--- ecx->VMMS structure
;--- edi=phys page for protected-mode stack
;--- ebp=vcpi entry (if ?USEVCPIEXIT is 1)

start proc stdcall public

	or edi,7
	mov eax, ds
	xchg edi,ds:[?PAGEDIR+?PAGETABSYS+0FFCh]  ;set the stack PTE

	mov ss, eax
	mov esp, ?SYSBASE + 400000h

if ?USEVCPIEXIT
	mov [vcpiofs],ebp
	mov [vcpisel],cs
endif

	push edi			;save old PTE for stack
	cmp [oldint20],0	;already initialized?
	jnz @F
	call InitVMM
	jc @@exit
@@:
	call Get_Cur_VM_Handle
	mov ebp,[ebx].cb_s.CB_Client_Pointer

	.if ([ecx].VMMS.wFlags & JLF_UNLOAD)
		call SearchJLM
		jc @@error
		mov ebx, [eax].VxD_Desc_Block.DDB_Reserved1
	.else
		call LoadJLM
		jc @@error
		mov esi,[ebx+3Ch]
		add esi, ebx	;point to NT header
		call DoFixups
		jc @@error
		call FreeDiscardableSections
		call SetDDB
	.endif

;--- call the modules entry point

	mov esi,[ebx+3Ch]
	add esi, ebx	;point to NT header

	mov edi, ebx
	mov eax, [esi].IMAGE_NT_HEADERS.OptionalHeader.AddressOfEntryPoint
	add eax, edi
	push edi
	push esi
	test [esi].IMAGE_NT_HEADERS.FileHeader.Characteristics, IMAGE_FILE_DLL
	jz isapp
	test [ecx].VMMS.wFlags, JLF_UNLOAD
	jz @@isload
	push ecx
	push DLL_PROCESS_DETACH
	push edi
	call eax
	pop esi
	pop edi
	and eax, eax
	jz @@exit
	jmp @@unload
@@isload:
	push ecx
	push DLL_PROCESS_ATTACH
	push edi
isapp:
	call eax
	pop esi
	pop edi
	and eax, eax
	jz @@unload
	test [esi].IMAGE_NT_HEADERS.FileHeader.Characteristics, IMAGE_FILE_DLL
	jnz @@exit
@@unload:
	call UnloadJLM
	jmp @@exit
@@error:
	xor eax,eax
@@exit:
	@DbgOutS <"start exit (returns to v86)",10>,?INITDBG
	mov edx, eax	;save returncode in edx
	pop eax

	lea esp, [ebp].Client_Reg_Struc.Client_EIP
	mov ds:[?PAGEDIR+?PAGETABSYS+0FFCh], eax	;restore stack PTE

	mov eax, cr3	;flush TLB to validate the just restored stack PTE
	mov cr3, eax

if ?USEVCPIEXIT
	mov ax,0DE0Ch
	call [vcpi]
else
	iretd
endif
	align 4
start endp

;--- ecx=VMMS
;--- ebx=module handle

SetDDB proc
	pushad
	movzx esi, [ecx].VMMS.pDDB
	and esi, esi
	jz exit
	@DbgOutS <"SetDDB: calling VMM_Add_DDB",10>,?PEDBG
	mov eax, [ebx+3Ch]
	add eax, ebx
	mov eax,[eax].IMAGE_NT_HEADERS.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT*sizeof IMAGE_DATA_DIRECTORY].VirtualAddress
	add eax, ebx
	mov eax,[eax].IMAGE_EXPORT_DIRECTORY.AddressOfFunctions
	add eax, ebx
	mov esi, [eax]
	add esi, ebx
	mov [esi].VxD_Desc_Block.DDB_Reserved1, ebx	;store module in DDB
	call VMM_Add_DDB
exit:
	mov [ebx+38h],esi	;store DDB in module
	popad
	ret
	align 4
SetDDB endp

;--- search a JLM
;--- to find a JLM, it must have an associated DDB with an ID
;--- out: NC: EAX=module handle
;---       C: eax=return code
;--- ECX is preserved

SearchJLM proc

	movzx edi, [ecx].VMMS.pDDB
	and edi, edi
	jz error
	movzx eax, word ptr [ebp].Client_Reg_Struc.Client_DS
	shl eax, 4
	add edi, eax
	movzx eax,[edi].VxD_Desc_Block.DDB_Req_Device_Number
	push ecx
	call FindDevice
	pop ecx
	jc error2
	@DbgOutS <"SearchJLM: device found",10>,?PEDBG
	ret
error2:
	@DbgOutS <"SearchJLM: device not found",10>,?PEDBG
error:
	@DbgOutS <"SearchJLM: no DDB",10>,?PEDBG
	mov cx,2      ;not found
	stc
	ret
	align 4

SearchJLM endp

;--- load a JLM
;--- 1. find an address space in system region
;--- 2. commit the pages
;--- 3. read the file with nested execution
;---    fixups aren't resolved yet!
;--- inp: ECX -> VMMS, EBP -> client regs
;--- out: NC if ok
;---    EBX=module handle
;---    ESI=DDB

LoadJLM proc
	@DbgOutS <"LoadJLM enter",10>,?PEDBG
	pushad
	call SearchJLM	;already loaded?
	jnc error3
	mov ecx,[esp].PUSHADS.rECX ;restore ecx
	movzx edi, [ecx].VMMS.pHdr
	movzx eax, word ptr [ebp].Client_Reg_Struc.Client_DS
	shl eax, 4
	add edi, eax
	mov ax, [ecx].VMMS.hFile
	mov word ptr [ebp].Client_Reg_Struc.Client_EBX, ax
	mov bx, [ecx].VMMS.pBuffer
	mov esi, [edi].IMAGE_NT_HEADERS.OptionalHeader.SizeOfImage
	shr esi, 12
	push 0
	push esi
	push PR_SYSTEM
	call _PageReserve
	add esp,3*4
	cmp eax,-1
	jz error
	mov [esp].PUSHADS.rEBX, eax   ;linear address
	shr eax, 12
	push PC_FIXED or PC_WRITEABLE
	push 0
	push PD_ZEROINIT
	push esi
	push eax
	call _PageCommit
	add esp,5*4
	and eax, eax
	jz error
	mov esi,[edi].IMAGE_NT_HEADERS.OptionalHeader.SizeOfHeaders
	.if (esi > 1000h)
		mov esi, 1000h
	.endif
	xor eax, eax
	mov edi, [esp].PUSHADS.rEBX
	call ReadFile
	jc error
	add edi, [edi+3Ch]
	movzx ecx, [edi].IMAGE_NT_HEADERS.FileHeader.NumberOfSections
	add edi, sizeof IMAGE_NT_HEADERS
	.while (ecx)
		push ecx
		push edi
		mov ecx, [edi].IMAGE_SECTION_HEADER.SizeOfRawData
		mov eax, [edi].IMAGE_SECTION_HEADER.PointerToRawData
		mov edi, [edi].IMAGE_SECTION_HEADER.VirtualAddress
		add edi, [esp+2*4].PUSHADS.rEBX
		.while (ecx)
			mov esi, ecx
			.if (esi > 1000h)
				mov esi, 1000h
			.endif
			push ecx
			push eax
			call ReadFile
			jc error2
			pop eax
			pop ecx
			add eax, esi
			add edi, esi
			sub ecx, esi
		.endw
		pop edi
		pop ecx
		add edi, sizeof IMAGE_SECTION_HEADER
		dec ecx
	.endw
	popad
	ret
error3:
	stc
	popad
	mov cx,0B7h	;already exists
	ret
error2:
	add esp,4*4
error:
	stc
	popad
	mov cx,8	;memory error
	ret
	align 4
LoadJLM endp

;--- unload the JLM's memory block
;--- inp: EDI=linear address

UnloadJLM proc
	pushad
	@DbgOutS <"UnloadJLM enter",10>,?PEDBG
	mov edi,[edi+38h]
	and edi,edi
	jz @F
	@DbgOutS <"UnloadJLM: calling VMM_Remove_DDB",10>,?PEDBG
	call VMM_Remove_DDB
@@:
	mov edi,[esp].PUSHADS.rEDI
	push 0     ;flags
	push edi
	call _PageFree
	add esp, 2*4
	popad
	ret
	align 4
UnloadJLM endp

;--- read a file portion (4kB)
;--- eax = file pos
;--- si = bytes to read
;--- bx = buffer offset
;--- edi = target addr
;--- Client_BX = file handle
;--- Client_DS = buffer segment

ReadFile proc

	mov word ptr [ebp].Client_Reg_Struc.Client_EDX, ax
	shr eax, 16
	mov word ptr [ebp].Client_Reg_Struc.Client_ECX, ax
	mov word ptr [ebp].Client_Reg_Struc.Client_EAX, 4200h
	call Begin_Nest_Exec
	mov eax, 21h
	call Exec_Int
	test [ebp].Client_Reg_Struc.Client_EFlags,1
	.if (ZERO?)
		mov word ptr [ebp].Client_Reg_Struc.Client_ECX, si
		mov word ptr [ebp].Client_Reg_Struc.Client_EDX, bx
		mov byte ptr [ebp].Client_Reg_Struc.Client_EAX+1, 3Fh
		mov eax, 21h
		call Exec_Int
		test [ebp].Client_Reg_Struc.Client_EFlags,1
		.if (ZERO?)
			movzx ecx, si
			push esi
			movzx esi, word ptr [ebp].Client_Reg_Struc.Client_DS
			shl esi, 4
			movzx ebx, bx
			add esi, ebx
			shr ecx, 2
			push edi
			rep movsd
			pop edi
			pop esi
		.endif
	.endif
	call End_Nest_Exec
	mov ah,byte ptr [ebp].Client_Reg_Struc.Client_EFlags
	sahf
	ret
	align 4
ReadFile endp

;--- walk relocations for PE binary loaded at linear address EBX
;--- inp: EBX = module handle, ESI=IMAGE_NT_HEADERS

DoFixups proc 

	@DbgOutS <"DoFixups enter",10>,?INITDBG
	pushad
	mov edx,ebx
	mov ebx,[esi].IMAGE_NT_HEADERS.OptionalHeader.ImageBase

	mov ebp, [esi].IMAGE_NT_HEADERS.OptionalHeader.SizeOfImage
	mov ecx, [esi].IMAGE_NT_HEADERS.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC*sizeof IMAGE_DATA_DIRECTORY].Size_
	jecxz done
	mov esi, [esi].IMAGE_NT_HEADERS.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC*sizeof IMAGE_DATA_DIRECTORY].VirtualAddress
	add esi,edx
nextblock:

	mov edi, [esi].IMAGE_BASE_RELOCATION.VirtualAddress
	cmp edi, ebp
	jnc done			;invalid relocations found!!!

	push ecx
	push esi
	push edx
	push ebx

	add edi, edx
	sub edx, ebx
	mov ecx, [esi].IMAGE_BASE_RELOCATION.SizeOfBlock
	add ecx, esi
	add esi, sizeof IMAGE_BASE_RELOCATION
	xor eax, eax
	.while (esi < ecx)
		lods word ptr [esi]
		mov bl,ah
		and ah,0Fh
		shr bl,4
		.if (bl == IMAGE_REL_BASED_HIGHLOW)
			add [edi+eax],edx
		.endif
	.endw
	pop ebx
	pop edx
	pop esi
	pop ecx
	mov eax,[esi].IMAGE_BASE_RELOCATION.SizeOfBlock
	add esi, eax
	sub ecx, eax
	ja nextblock
done:
	popad
	@DbgOutS <"DoFixups exit",10>,?INITDBG
	clc
	ret
	align 4
DoFixups endp

;--- free all objects marked as discardable
;--- inp: ESI -> IMAGE_NT_HEADERS
;--- inp: EBX = module base

FreeDiscardableSections proc 

	@DbgOutS <"FreeDiscardableSections enter",10>,?INITDBG
	pushad
	movzx ecx,[esi].IMAGE_NT_HEADERS.FileHeader.NumberOfSections
	lea edi,[esi+size IMAGE_NT_HEADERS]
	jecxz @@done
@@nextitem:
	test [edi].IMAGE_SECTION_HEADER.Characteristics, IMAGE_SCN_MEM_DISCARDABLE
	jz @@skipitem
	mov eax, [edi].IMAGE_SECTION_HEADER.VirtualAddress
	mov edx, [edi].IMAGE_SECTION_HEADER.Misc.VirtualSize
	add edx, 1000h-1
	shr edx, 12
	jz @@skipitem
	.if (eax)
		push ecx

		add eax, ebx
		shr eax, 12
		push 0
		push edx
		push eax
		call _PageDecommit
		add esp,3*4

		pop ecx
	.endif
@@skipitem:
	add edi, size IMAGE_SECTION_HEADER
	loop @@nextitem
@@done:
	popad
	@DbgOutS <"FreeDiscardableSections exit",10>,?INITDBG
	clc
	ret
	align 4
FreeDiscardableSections endp

if ?TRACE

;--- display DWORD in eax

VDWORDOUT proc
	push eax
	shr eax,16
	call VWORDOUT
	pop eax
VDWORDOUT endp
VWORDOUT proc
	push eax
	mov al,ah
	call VBYTEOUT
	pop eax
VWORDOUT endp
VBYTEOUT proc
	pushfd
	push eax
	mov ah,al
	shr al,4
	call VNIBOUT
	mov al,ah
	call VNIBOUT
	pop eax
	popfd
	ret
VBYTEOUT endp
VNIBOUT proc
	and al,0Fh
	cmp al,10
	sbb al,69H
	das
	jmp VPUTCHR
	align 4
VNIBOUT endp

VPUTCHR PROC
	push ds
	PUSHAD
	push ss
	pop ds
if ?USEMONO    
	mov edi,0B0000h
	mov ebx,7
else
	MOV EDI,0B8000h
	CMP BYTE ptr DS:[463h],0B4h
	JNZ @@IS_COLOR
	XOR DI,DI
@@IS_COLOR:
	movzx EBX, WORD PTR DS:[44Eh]
	ADD EDI, EBX
	MOVZX EBX, BYTE PTR DS:[462h]
endif
	mov esi, edi
	MOVZX ECX, BYTE PTR DS:[EBX*2+450h+1]	;ROW
if ?USEMONO
	MOV EAX, 80
else
	MOVZX EAX, WORD PTR DS:[44Ah]
endif
	MUL ECX
	MOVZX EDX, BYTE PTR DS:[EBX*2+450h]	;COL
	ADD EAX, EDX
	MOV DH,CL
	LEA EDI, [EDI+EAX*2]
	MOV AL, [ESP+1Ch]
	CMP AL, 10
	JZ @@NEWLINE
	MOV [EDI], AL
	MOV byte ptr [EDI+1], 07
	INC DL
if ?USEMONO
	cmp dl,80
else
	CMP DL, BYTE PTR DS:[44Ah]
endif
	JB @@OLDLINE
@@NEWLINE:
	MOV DL, 00
	INC DH
if ?USEMONO
	CMP DH, 24
else
	CMP DH, BYTE PTR DS:[484h]
endif
	JBE @@OLDLINE
	DEC DH
	CALL @@SCROLL_SCREEN
@@OLDLINE:
	MOV DS:[EBX*2+450h],DX
	POPAD
	pop ds
	RET
	align 4

;--- scroll screen up 1 line
;--- esi -> start screen

@@SCROLL_SCREEN:
	push es
	push ds
	pop es
	CLD
	mov edi,esi
if ?USEMONO
	mov eax,80
else
	movzx eax,word ptr ds:[44Ah]
endif
	push eax
	lea esi, [esi+2*eax]
if ?USEMONO
	mov CL, 24
else
	MOV CL, DS:[484h]
endif
	mul cl
	mov ecx,eax
	rep MOVS WORD PTR [EDI], WORD PTR [ESI]
	pop ecx
	mov ax,0720h
	rep stos WORD PTR [EDI]
	pop es
	retn
	align 4

VPUTCHR ENDP


VPRINTSTR PROC
	XCHG EBX,[ESP]
	PUSH EAX
@@NEXTCHAR:
	MOV AL,CS:[EBX]		; using CS prefix should always work here
	INC EBX
	CMP AL,0
	JZ @@DONE
	call VPUTCHR
	JMP @@NEXTCHAR
@@DONE:
	POP EAX
	XCHG EBX,[ESP]
	RET
	align 4
VPRINTSTR endp

endif

	end start


