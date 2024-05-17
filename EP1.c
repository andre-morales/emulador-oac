#include "driverEP1.h"
#include <stdio.h>
#include <stdint.h>

void pause();
void badInstruction(uint16_t pc, uint16_t instr);

int processa (short int* memory, int memSize) {
	// Definição dos registradores
	uint16_t rRI = 0;
	uint16_t rPC = 0;
	uint16_t rA = 0;
	uint16_t rB = 0;
	uint16_t rC = 0;
	uint16_t rD = 0;
	uint16_t rR = 0;
	uint16_t rPSW = 0;

	do {
		// Lê a instrução atual
		rRI = memory[rPC];

		// Extrai da instrução atual os 4 bits do código de operação
		uint8_t opcode = (rRI & 0xF000) >> 12;
		
		// HLT - Opcode (1111)b
		// Interrompe a execução do processador
		if (opcode == 0xF) break;
		
		switch(opcode) {
		// NOP - Opcode (0000)b
		// Não faz nada
		case 0x0:
			break;

		// Instrução desconhecida?
		default:
			badInstruction(rPC, rRI);	
		}
		
		// Incrementa o ponteiro para a próxima instrução
		rPC++;

		// Se o contador de programa ultrapassou o limite da memória, reinicie-o em 0.
		if (rPC >= memSize) rPC=0;
	// O programa para ao encontrar HLT
	} while ((rRI & 0xF000) != 0xF000);

	printf("\nCPU Parada.\n");
}

void badInstruction(uint16_t pc, uint16_t instr) {
	printf ("-- Instrução indefinida 0x%hx em 0x%hx --\n", instr, pc);
	pause();
}

void pause() {
	getchar();
}