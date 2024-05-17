#include "driverEP1.h"
#include <stdio.h>
#include <stdint.h>

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

void pause();
void badInstruction(Registers* r);
void dumpRegisters(Registers* regs);
void verifyAddress(Registers* r, uint16_t addr, uint16_t size);
void fault(Registers* r);

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

	do {
		// Lê a instrução atual
		r.RI = memory[r.PC];

		// Extrai da instrução atual os 4 bits do código de operação
		uint8_t opcode = (r.RI & 0xF000) >> 12;

		// Extrai o argumento X da instrução, usado nas operações LDA, STA, JMP e JNZ
		uint16_t argument = (r.RI & 0x0FFF);

		// HLT - Opcode (1111)b
		// Interrompe a execução do processador
		if (opcode == 0xF) break;
		
		switch(opcode) {
		// NOP -- Opcode (0000)b
		// Não faz nada
		case 0x0:
			break;

		// LDA(x) -- Opcode (0001)b
		// Carrega o acumulador (A) com o conteúdo da memória em X
		case 0x1:
			// Garante a validade do endereço de memória X
			verifyAddress(&r, argument, memSize);

			r.A = memory[argument];
			break;

		// JNZ(x) -- Opcode (0100)b
		// Se o acumulador for != de 0, armazena o endereço da próxima instrução em R e salta para
		// o endereço especificado no argumento X
		case 0x4:
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

		case 0x6:
			break;

		// Instrução desconhecida
		default:
			badInstruction(&r);	
		}
		
		// Incrementa o ponteiro para a próxima instrução
		r.PC++;

		// Se o contador de programa ultrapassou o limite da memória, reinicie-o em 0.
		if (r.PC >= memSize) r.PC=0;
	// O programa para ao encontrar HLT
	} while ((r.RI & 0xF000) != 0xF000);

	printf("\nCPU Halted.\n");
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

void dumpRegisters(Registers* regs) {
	printf("---- Program registers ----\n");
	printf("PC:  0x%hx\n", regs->PC);
	printf("RI:  0x%hx\n", regs->RI);
	printf("PSW: 0x%hx\n", regs->PSW);
	printf("\n");
	printf("A:   0x%hx\n", regs->A);
	printf("B:   0x%hx\n", regs->B);
	printf("C:   0x%hx\n", regs->C);
	printf("D:   0x%hx\n", regs->D);
	printf("\n");
	printf("R:   0x%hx\n", regs->R);
}

void pause() {
	getchar();
}