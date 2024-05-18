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
void guardAddress(Emul* emul, uint16_t addr);
void fault(Registers* r);
void faultMsg(Emul* emul, const char* fmt, ...);
int executeInstruction(Emul* emul, uint8_t opcode, uint16_t argument);
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
	"PSW", // 111b
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

		// Imprime a instrução atual
		printf("[%3Xh] %X_%03X: %s ", r.PC, opcode, argument, INSTRUCTION_NAMES[opcode]);

		// Executa a instrução com seu argumento opcional
		int result = executeInstruction(&emul, opcode, argument);
		if (result == 1) {
			break;
		}

		printf("\n");

		// Incrementa o ponteiro para a próxima instrução
		r.PC++;

		// Se o contador de programa ultrapassou o limite da memória, reinicie-o em 0.
		if (r.PC >= memSize) r.PC = 0;

	// O programa para ao encontrar HLT
	} while ((r.RI & 0xF000) != 0xF000);

	printf("\nCPU Halted.\n");
}

int executeInstruction(Emul* emul, uint8_t opcode, uint16_t argument) {
	Registers* regs = emul->registers;
	uint16_t* memory = emul->memory;

	switch(opcode) {
	// NOP -- Opcode (0000)b
	// Não faz nada
	case 0x0:
		break;

	// LDA(x) -- Opcode (0001)b
	// Carrega o acumulador (A) com o conteúdo da memória em X
	case 0x1: {
		printf("[%Xh]", argument);

		// Garante a validade do endereço de memória X
		guardAddress(emul, argument);

		regs->A = memory[argument];
		break;
	}

	// STA(x) -- Opcode (0010)b
	// Armazena no endereço imediato X o valor do acumulador A
	case 0x2: {
		printf("[%Xh]", argument);

		// Garante a validade do endereço de memória X
		guardAddress(emul, argument);

		memory[argument] = regs->A;
		break;
	}

	// JMP(x) -- Opcode (0011)b
	// Pula incondicionalmente para o endereço imediato X
	/*case 0x3: {
		printf("%Xh", argument);

		// Garante que o destino X de salto é válido
		guardAddress(emul, argument);

		// Salva R como o endereço da próxima instrução
		regs->R = regs->PC + 1;

		// Modifica o contador pro endereço X - 1. Subtraímos 1 pois o loop já incrementa
		// em 1 o PC.
		regs->PC = argument - 1;
		break;
	}*/

	// JNZ(x) -- Opcode (0100)b
	// Se o acumulador for != de 0, armazena o endereço da próxima instrução em R e salta para
	// o endereço especificado no argumento X
	case 0x4: {
		printf("%Xh", argument);

		// Garante que o destino X de salto é válido
		guardAddress(emul, argument);

		if (regs->A != 0) {
			// Salva R como o endereço da próxima instrução
			regs->R = regs->PC + 1;

			// Modifica o contador pro endereço X - 1. Subtraímos 1 pois o loop já incrementa
			// em 1 o PC.
			regs->PC = argument - 1;
		}

		break;
	}

	// ARIT(x) -- Opcode (0110)b
	// Executa uma operação aritmética.
	case 0x6: {
		doArit(emul, argument);
		break;
	}

	// HLT - Opcode (1111)b
	// Interrompe a execução do processador
	case 0xF:
		return 1;

	// Instrução desconhecida
	default: {
		printf(" ??? 0x%X : 0x%X\n", opcode, argument);

		badInstruction(regs);	
		break;
	}
	}
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
	uint16_t* regOp2;

	// Se o bit mais significante do operando 2 for 0, o operando é considerado como o próprio número 0.
	if ((bitsOp2 & 0b100) == 0) {
		regOp2 = NULL;
	// Caso contrário, os outros dois bits selecionarão um registrador de A, B, C ou D
	} else {
		regOp2 = getRegisterWithBits(emul->registers, bitsOp2 & 0b011);
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
	printf("%s ", (regOp2) ? REGISTER_NAMES[bitsOp2 & 0b011] : "zero");

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
	case 0x7:
		return &r->PSW;
	default:
		return NULL;
	}
}

// Verifica se um enderço se memória está dentro dos limites possíveis do tamanho da memória
// do emulador. Se o endereço estiver fora do limite, causa uma falha.
void guardAddress(Emul* emul, uint16_t addr) {
	if (addr >= emul->memorySize) {
		printf("[!] Memory access out of bounds (%x)\n", addr);
		fault(emul->registers);
	}
}

void badInstruction(Registers* r) {
	printf ("[!] Bad instruction 0x%hx at 0x%hx --\n", r->RI, r->PC);
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