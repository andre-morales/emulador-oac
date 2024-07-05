BEGIN:
	jmp :start

i_array_size:
	dw 20

i_array:
	dw 12
	dw 1
	dw 14
	dw 11
	dw 5
	dw 20
	dw 3
	dw 19
	dw 4
	dw 17
	dw 9
	dw 16
	dw 2
	dw 6
	dw 13
	dw 7
	dw 15
	dw 0
	dw 18
	dw 10

.0x200
i_array_ptr:
	dw :i_array

; Constants
c_zero:
	dw 0
c_one:
	dw 1
c_greater_mask:
	dw 0x800
c_less_mask:
	dw 0x2000
c_load_opcode:
	dw 0x1000
c_store_opcode:
	dw 0x2000

; Backup area for registers
bk_a:
	dw 0
bk_b:
	dw 0
bk_r:
	dw 0
bk_x:
	dw 0

; Program variables
counter:
	dw 0
subCounter:
	dw 0
firstNum:
	dw 0
secondNum:
	dw 0
firstPtr:
	dw 0
secondPtr:
	dw 0

.0x240
start:
	; The main loop counter counts forwards from 0 to N - 1
	lda :c_zero
	sta :counter
	
	mainLoop:
		; Set B = array_size
		lda :i_array_size
		calc b = a

		; Make A = array_size - i - 1
		lda :counter
		calc a = b - a
		jmp :decrement_a

		; Save A as the sub loop bound
		sta :subCounter

		; Set D = &array
		lda :i_array_ptr
		calc d = a
		subLoop:
			; Save ptr to first number
			calc a = d
			sta :firstPtr

			; Fetch first number and save it in B
			jmp :dload_a
			sta :firstNum
			calc b = a

			; Save ptr to second number
			jmp :increment_d
			calc a = d
			sta :secondPtr

			; Fetch it and save it in C
			jmp :dload_a
			sta :secondNum
			calc c = a
			
			; Perform comparison, isolate 'greater than' bit with mask and check it
			calc a = b - c
			calc b = psw
			lda :c_greater_mask
			
			; If the first number and second number comparison succeded, perform a swap
			calc a = a & b
			jnz :doSwap
			doSwap.ret:

			; Decrement counter for subloop
			lda :subCounter
			jmp :decrement_a
			sta :subCounter
		jnz :subLoop

		; Calculate i++
		lda :counter
		jmp :increment_a
		sta :counter
		
		; Make B = i, and A = N - 1
		calc b = a
		lda :i_array_size
		jmp :decrement_a

		; Compare B(i) < A(N) - 1
		calc a = b + a
		calc b = psw
		lda :c_less_mask
		calc a = b & a
	jnz :mainLoop

	; Ordering is finished
	hlt

; Swap the first and second number
doSwap:
	lda :firstNum
	sta :store
	lda :secondPtr
	jmp :dstore_a

	lda :secondNum
	sta :store
	lda :firstPtr
	jmp :dstore_a
jmp :doSwap.ret

; Axuiliary functions begin here
.0x300
increment_d:
	lda :c_one
	calc d = d + a
ret

decrement_a:
	; Save A
	sta :bk_a

	; Save B
	calc a = b
	sta :bk_b

	; Restore A, and calculate A -= 1
	lda :bk_a
	calc b = a
	lda :c_one
	calc a = b - a

	; Save Result
	sta :decrement_a.tmp

	; Restore B
	lda :bk_b
	calc b = a
	
	; Restore result
	lda :decrement_a.tmp
ret 

decrement_a.tmp:
	dw 0

increment_a:
	; Save A
	sta :bk_a

	; Save B
	calc a = b
	sta :bk_b

	; Restore A, and calculate A += 1
	lda :bk_a
	calc b = a
	lda :c_one
	calc a = a + b

	; Save Result
	sta :increment_a.tmp

	; Restore B
	lda :bk_b
	calc b = a
	
	; Restore result
	lda :increment_a.tmp
ret 

increment_a.tmp:
	dw 0

decrement_b:
	lda :c_one
	calc b = b - a	
ret

; Place the number to be dynamically stored here
store:
	dw 0

; Dynamic Store Function
dstore_a:
	; Save parameter in A
	sta :bk_a
	
	; Save R and B
	calc a = r
	sta :bk_r
	calc a = b
	sta :bk_b

	; Set B = Load Address
	lda :bk_a
	calc b = a

	; Calculate A (0x2000) += B (adrr)
	lda :c_store_opcode
	calc a = a + b
	sta :store_a_rec

	; Load the number to store and go to the constructed function
	lda :store
	jmp :store_a_rec

	store_a_ret:
	lda :bk_b
	calc b = a
	lda :bk_r
	calc r = a
	
	lda :bk_a
ret

store_a_rec:
	dw 0
	jmp :store_a_ret

; Dynamic Load Function
dload_a:
	; Save parameter in A
	sta :bk_a
	
	; Save R and B
	calc a = r
	sta :bk_r
	calc a = b
	sta :bk_b

	; Set B = Load Address
	lda :bk_a
	calc b = a

	; Calculate A (0x1000) += B (adrr)
	lda :c_load_opcode
	calc a = a + b
	sta :sub_a_rec
	jmp :sub_a_rec

	sub_a_ret:
	sta :bk_a

	lda :bk_b
	calc b = a
	lda :bk_r
	calc r = a
	
	lda :bk_a
ret

sub_a_rec:
	dw 0
	jmp :sub_a_ret

