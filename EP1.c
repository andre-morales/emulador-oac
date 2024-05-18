#include "driverEP1.h"
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

// Definição dos registradores do processador
typedef struct {
	uint16_t RI;
	uint16_t PC;
	uint16_t A;
	uint16_t B;
	uint16_t C;
	uint16_t D;
	uint16_t R;
	uint16_t PSW;
} Registers;

typedef struct {
	Registers* registers;
	uint16_t* memory;
	uint16_t memorySize;
} Emul;

void pause();
void badInstruction(Registers* r);
void dumpRegisters(Registers* regs);
void verifyAddress(Registers* r, uint16_t addr, uint16_t size);
void fault(Registers* r);
void faultMsg(Emul* emul, const char* fmt, ...);
void doArit(Emul* emul, uint16_t argument);
uint16_t* getRegisterWithBits(Registers* r, uint8_t code);

static const char* const INSTRUCTION_NAMES[] = {
	"NOP",  // 0000b
	"LDA",  // 0001b
	"STA",  // 0010b
	"JMP",  // 0011b
	"JNZ",  // 0100b
	"RET",  // 0101b
	"ARIT", // 0110b
	"???",  // 0111b
	"???",  // 1000b
	"???",  // 1001b
	"???",  // 1010b
	"???",  // 1011b
	"???",  // 1100b
	"???",  // 1101b
	"???",  // 1110b
	"HLT"   // 1111b
};

static const char* const REGISTER_NAMES[] = {
	"A", // 000b
	"B", // 001b
	"C", // 010b
	"D", // 011b
	"?", // 100b
	"?", // 101b
	"?", // 110b
	"?", // 111b
};

static const char* const ARIT_OP_NAMES[] = {
	"SETF", // 000b
	"SET0", // 001b
	"NOT",  // 010b
	"AND",  // 011b
	"OR",   // 100b
	"XOR",  // 101b
	"ADD",  // 110b
	"SUB"   // 111b
};

int processa (short int* m, int memSize) {
	uint16_t* memory = (uint16_t*)m;

	// Inicialização dos registradores
	Registers r;
	r.RI = 0;
	r.PC = 0;
	r.A = 0;
	r.B = 0;
	r.C = 0;
	r.D = 0;
	r.R = 0;
	r.PSW = 0;

	// Estrutura do emulador
	Emul emul;
	emul.memory = memory;
	emul.memorySize = memSize;
	emul.registers = &r;

	do {
		// Lê a instrução atual
		r.RI = memory[r.PC];		
		// Extrai da instrução atual os 4 bits do código de operação
		uint8_t opcode = (r.RI & 0xF000) >> 12;

		// Extrai o argumento X da instrução, usado nas operações LDA, STA, JMP e JNZ
		uint16_t argument = (r.RI & 0x0FFF);

		// Imprime o program counter atual
		printf("[%3xh] %X_%03X: %s ", r.PC, opcode, argument, INSTRUCTION_NAMES[opcode]);

		// HLT - Opcode (1111)b
		// Interrompe a execução do processador
		if (opcode == 0xF) {
			break;
		}
		
		switch(opcode) {
		// NOP -- Opcode (0000)b
		// Não faz nada
		case 0x0:
			break;

		// LDA(x) -- Opcode (0001)b
		// Carrega o acumulador (A) com o conteúdo da memória em X
		case 0x1:
			printf("[%Xh]", argument);

			// Garante a validade do endereço de memória X
			verifyAddress(&r, argument, memSize);

			r.A = memory[argument];
			break;

		case 0x2:
			printf("[%Xh]", argument);

			// Garante a validade do endereço de memória X
			verifyAddress(&r, argument, memSize);

			memory[argument] = r.A;
			break;

		// JNZ(x) -- Opcode (0100)b
		// Se o acumulador for != de 0, armazena o endereço da próxima instrução em R e salta para
		// o endereço especificado no argumento X
		case 0x4:
			printf("[%x]", argument);

			// Garante que o destino X de salto é válido
			verifyAddress(&r, argument, memSize);

			if (r.A != 0) {
				// Salva R como o endereço da próxima instrução
				r.R = r.PC + 1;

				// Modifica o contador pro endereço X - 1. Subtraímos 1 pois o loop já incrementa
				// em 1 o PC.
				r.PC = argument - 1;
			}

			break;

		// ARIT(x) -- Opcode (0110)b
		// Executa uma operação aritmética.
		case 0x6:
			doArit(&emul, argument);
			break;

		// Instrução desconhecida
		default:
			printf(" ??? 0x%i : 0x%i\n", opcode, argument);

			badInstruction(&r);	
		}
		
		printf("\n");

		// Incrementa o ponteiro para a próxima instrução
		r.PC++;

		// Se o contador de programa ultrapassou o limite da memória, reinicie-o em 0.
		if (r.PC >= memSize) r.PC=0;

	// O programa para ao encontrar HLT
	} while ((r.RI & 0xF000) != 0xF000);

	printf("\nCPU Halted.\n");
}

void doArit(Emul* emul, uint16_t argument) {
	// Extrai os 3 bits que determinam a operação aritmética a realizar
	uint8_t bitsOpr = (argument & 0b111000000000) >> 9;

	// Extrai os 3 bits que definem o registrador destino da operação
	uint8_t bitsDst = (argument & 0b000111000000) >> 6;

	// Extrai os bits que definem o registrador do primeiro operando
	uint8_t bitsOp1 = (argument & 0b000000111000) >> 3;

	// Extrai os bits que definem o registrador do segundo operando
	uint8_t bitsOp2 =  argument & 0b000000000111;

	// Obtém o registrador destino usando os bits de Res
	uint16_t* regDst = getRegisterWithBits(emul->registers, bitsDst);
	if (!regDst) {
		faultMsg(emul, "Invalid arit register destination code: %i\n", bitsDst);
		return;
	}

	// Obtém o registrador operando usando os bits de Op1
	uint16_t* regOp1 = getRegisterWithBits(emul->registers, bitsOp1);
	if (!regOp1) {
		faultMsg(emul, "Invalid arit register op1 code: %i\n", bitsOp1);
		return;
	}

	// Obtém o registrador operando usando os bits de Op2
	// Se o bit mais significante do operando 2 estiver setado. O operando 2 será o número 0,
	// aqui considerado como NULL, caso contrário, será o registrador mesmo
	uint16_t* regOp2;
	if (bitsOp2 & 0b100 == 0b100) {
		regOp2 = NULL;
	} else {
		regOp2 = getRegisterWithBits(emul->registers, bitsOp2);
		if (!regOp2) {
			faultMsg(emul, "Invalid arit register op2 code: %i\n", bitsOp2);
			return;
		}	
	}	

	// Obtém os valores dos registradores apontados por Op1 e Op2.
	// Novamente, se o registrador de Op2 for NULL, é considerado o valor imediato 0.
	uint16_t op1 = *regOp1;
	uint16_t op2 = (regOp2) ? *regOp2 : 0;

	printf("%s, ", ARIT_OP_NAMES[bitsOpr]);
	printf("%s, ", REGISTER_NAMES[bitsDst]);
	printf("%s, ", REGISTER_NAMES[bitsOp1]);
	printf("%s ", (regOp2)?REGISTER_NAMES[bitsOp2]:"zero");

	switch(bitsOpr) {
	// Set FFFFh
	case 0:
		//printf("SETF, ");
		break;
	case 1:
		//printf("SET0, ");
		break;
	case 2:
		//printf("NOT, ");
		break;
	case 3:
		//printf("AND, ");
		break;
	case 4:
		//printf("OR, ");
		break;
	case 5:
		//printf("XOR, ");
		break;
	case 6:
		//printf("ADD, ");
		break;
	case 7:
		//printf("SUB, ");
		break;
	}
}

uint16_t* getRegisterWithBits(Registers* r, uint8_t code) {
	switch (code) {
	case 0x0:
		return &(r->A);
	case 0x1:
		return &r->B;
	case 0x2:
		return &r->C;
	case 0x3:
		return &r->D;
	//case 0x6:
	//	return &r->R;
	default:
		return NULL;
	}
}

void verifyAddress(Registers* r, uint16_t addr, uint16_t size) {
	if (addr >= size) {
		printf("[!] Memory access out of bounds (%x)\n", addr);
		fault(r);
	}
}

void badInstruction(Registers* r) {
	printf ("[!] Bad instruction 0x%hx em 0x%hx --\n", r->RI, r->PC);
	fault(r);
}

void fault(Registers* r) {
	printf("\n-- Fault: Execution paused. --\n");
	dumpRegisters(r);
	pause();
}

void faultMsg(Emul* emul, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printf("\n[!] Fault: ");
	vfprintf(stdout, fmt, args);
	dumpRegisters(emul->registers);
	pause();
	va_end(args);
}

void dumpRegisters(Registers* regs) {
	printf("---- Program registers ----\n");
	printf("PC:  0x%04hx\n", regs->PC);
	printf("RI:  0x%04hx\n", regs->RI);
	printf("PSW: 0x%04hx\n", regs->PSW);
	printf("R:   0x%04hx\n", regs->R);
	printf("\n");
	printf("A:   0x%04hx\n", regs->A);
	printf("B:   0x%04hx\n", regs->B);
	printf("C:   0x%04hx\n", regs->C);
	printf("D:   0x%04hx\n", regs->D);	
}

void pause() {
	getchar();
}