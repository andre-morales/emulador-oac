#include "driverEP1.h"
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

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

// Definição da estrutura do emulador
typedef struct {
	Registers* registers;
	uint16_t* memory;
	uint16_t memorySize;
	bool breaking;
} Emul;

// Definição de string buffer para a formatação de mensagens
typedef struct {
	char* buffer;
	size_t position;
	size_t size;
	size_t capacity;
} StringBuffer;

// Funções da interface e execução do emulador
void waitUserCommand();
void badInstruction(Registers* r);
void dumpRegisters(Registers* regs);
void guardAddress(Emul* emul, uint16_t addr);
void fault(Registers* r);
void faultMsg(Emul* emul, const char* fmt, ...);
int executeInstruction(Emul* emul, uint8_t opcode, uint16_t argument);
void doArit(Emul* emul, uint16_t argument);
uint16_t* getRegisterWithBits(Registers* r, uint8_t code);
StringBuffer disassembly(uint16_t instruction);
void pause();

// Funções auxiliares de manipulação de strings
void stbInit(StringBuffer*);
bool stbPrint(StringBuffer*, const char* fmt, ...);
void stbFree(StringBuffer*); 

// Tabelas estáticas
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

// Estrutura global de emulação
Emul emulator;

int processa (short unsigned int* m, int memSize) {
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
	emulator.memory = memory;
	emulator.memorySize = memSize;
	emulator.registers = &r;
	emulator.breaking = true;

	do {
		// Lê a instrução atual
		r.RI = memory[r.PC];	

		// Extrai da instrução atual os 4 bits do código de operação
		uint8_t opcode = (r.RI & 0xF000) >> 12;

		// Extrai o argumento X da instrução, usado nas operações LDA, STA, JMP e JNZ
		uint16_t argument = (r.RI & 0x0FFF);

		// Imprime a posição e o código da instrução atual
		printf("[%3Xh] %X.%03X: ", r.PC, opcode, argument);

		// Imprime o disassembly da instrução
		StringBuffer instructionStr = disassembly(r.RI);
		printf("%s\n", instructionStr.buffer);
		stbFree(&instructionStr);

		// Se o emulador está em modo step-through, permita ao usuário decidir o que fazer antes
		// de executar qualquer instrução
		if (emulator.breaking) {
			waitUserCommand();
		}

		// Executa a instrução com seu argumento opcional
		int result = executeInstruction(&emulator, opcode, argument);
		if (result == 1) {
			break;
		}

		// Incrementa o ponteiro para a próxima instrução
		r.PC++;

		// Se o contador de programa ultrapassou o limite da memória, reinicie-o em 0.
		if (r.PC >= memSize) r.PC = 0;

	// O programa para ao encontrar HLT
	} while ((r.RI & 0xF000) != 0xF000);

	printf("\nCPU Halted.\n");

	return 0;
}

void waitUserCommand() {
	const size_t BUFFER_SIZE = 128;
	static char commandBuffer1[128] = { 0 };
	static char commandBuffer2[128] = { 0 };

	static char* commandBuffer = commandBuffer1;
	static char* lastCommand = commandBuffer2;

	// Loop infinito apenas interrompido quando o usuário digitar um comando válido
	while (true) {	
		printf(" >> ");
		
		// Lê uma linha de comando		
		fgets(commandBuffer, BUFFER_SIZE, stdin);

		// Remove a quebra de linha da string
		for (int i = 0; i < BUFFER_SIZE; i++) {
			if (commandBuffer[i] == '\n') {
				commandBuffer[i] = '\0';
				break;
			}
		}

		// Se a linha digitada for completamente vazia, o usuário apenas apertou enter e o comando
		// anterior será executado novamente
		char cmdLine[BUFFER_SIZE];
		if (commandBuffer[0] == '\0') {
			strncpy(cmdLine, lastCommand, BUFFER_SIZE);
		} else {
			strncpy(cmdLine, commandBuffer, BUFFER_SIZE);

			// Faz o swap do buffer de comando anterior e atual
			char* tmp = commandBuffer;
			commandBuffer = lastCommand;
			lastCommand = tmp;
		}

		// Obtém o comando principal antes do espaço
		char* command = strtok(cmdLine, " ");

		// Comando step: Permite executar a próxima instrução mas continua no modo step-through
		if (strcmp(command, "s") == 0) {
			return;
		}

		// Comando continue: Desativa o modo step-through do emulador
		if (strcmp(command, "c") == 0) {
			printf("Resuming execution...\n");
			emulator.breaking = false;
			return;	
		}

		// Comando registers: Imprime o conteúdo de todos os registradores
		if (strcmp(command, "r") == 0) {
			dumpRegisters(emulator.registers);
		}

		// Comando disassembly: Imprime a instrução atual e a sua posição
		if (strcmp(command, "d") == 0) {
			uint32_t counter = emulator.registers->PC;
			uint32_t instruction = emulator.registers->RI;
				
			uint8_t opcode = (instruction & 0xF000) >> 12;
			uint16_t argument = (instruction & 0x0FFF);

			StringBuffer sb = disassembly(instruction);
			printf("[%3Xh] %X.%03X: %s\n", counter, opcode, argument, sb.buffer);
			stbFree(&sb);
		}
	}
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
		// Garante a validade do endereço de memória X
		guardAddress(emul, argument);

		regs->A = memory[argument];
		break;
	}

	// STA(x) -- Opcode (0010)b
	// Armazena no endereço imediato X o valor do acumulador A
	case 0x2: {
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
		badInstruction(regs);	
		break;
	}
	}

	return 0;
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

StringBuffer disassembly(uint16_t instruction) {
	// Inicializa um buffer para as strings impressas nessa função
	StringBuffer bufferStg;
	StringBuffer* buffer = &bufferStg;
	stbInit(buffer);

	// Extrai o opcode e o argumento X da instrução
	uint8_t opcode = (instruction & 0xF000) >> 12;
	uint16_t argument = (instruction & 0x0FFF);

	// Imprime o nome da instrução
	stbPrint(buffer, "%s ", INSTRUCTION_NAMES[opcode]);

	switch(opcode) {
	// NOP -- 0000b
	case 0x0:
		break;

	// LDA(x) -- 0001b
	case 0x1:
		stbPrint(buffer, "[%Xh]", argument);
		break;

	// STA(x) -- 0010b
	case 0x2:
		stbPrint(buffer, "[%Xh]", argument);
		break;

	// JNZ(x) -- 0100b
	case 0x4:
		stbPrint(buffer, "%Xh", argument);
		break;

	// ARIT(opr, dst, op1, op2) -- 0110b
	case 0x6:
		// Extração dos bits respectivamente:
		// 3 bits que determinam a operação aritmética a realizar
		// 3 bits que definem o registrador destino da operação
		// 3 bits que definem o registrador do primeiro operando
		// 3 bits que definem o registrador do segundo operando
		uint8_t bitsOpr = (argument & 0b111000000000) >> 9;
		uint8_t bitsDst = (argument & 0b000111000000) >> 6;
		uint8_t bitsOp1 = (argument & 0b000000111000) >> 3;	
		uint8_t bitsOp2 =  argument & 0b000000000111;

		// Se o bit mais significante do operando 2 for 0, o operando em si é o 0 imediato
		bool op2zero = (bitsOp2 & 0b100) != 0;

		// Imprime em sequência OPERAÇÃO, RES, OP1, OP2
		stbPrint(buffer, "%s, ", ARIT_OP_NAMES[bitsOpr]);
		stbPrint(buffer, "%s, ", REGISTER_NAMES[bitsDst]);
		stbPrint(buffer, "%s, ", REGISTER_NAMES[bitsOp1]);
		stbPrint(buffer, "%s ", (op2zero) ? "zero" : REGISTER_NAMES[bitsOp2 & 0b011]);
		break;

	// HLT - 1111b
	case 0xF:
		break;

	// Instrução desconhecida
	default:
		stbPrint(buffer, " ??? 0x%X : 0x%X\n", opcode, argument);
		break;
	}

	return bufferStg;
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

// -- Funções de manipulação de StringBuffer --
void stbInit(StringBuffer* sb) {
	sb->capacity = 512;
	sb->position = 0;
	sb->size = 0;
	sb->buffer = (char*) malloc(sizeof(char) * sb->capacity);
}

bool stbPrint(StringBuffer* sb, const char* fmt, ...) {
	char* ptr = sb->buffer + sb->position;
	size_t len = sb->capacity - sb->size;

	va_list args;
	va_start(args, fmt);
	int written = vsnprintf(ptr, len, fmt, args);
	va_end(args);

	// Erro de formatação. Não foi escrito nada
	if (written < 0) return false;

	// Tamanho do buffer insuficiente
	if (written >= len) {
		return false;
	}

	// Escrita bem sucedida
	sb->position += written;
	return true;
}

void stbFree(StringBuffer* sb) {
	free(sb->buffer);
	sb->size = 0;
	sb->capacity = 0;
	sb->position = 0;
}