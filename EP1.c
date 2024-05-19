#include "driverEP1.h"
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>

#define START_IN_BREAKING_MODE 1

extern char* HELP_MSG;

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
	int stepsLeft;
} Emul;

// Definição de string buffer para a formatação de mensagens
typedef struct {
	char* buffer;
	size_t position;
	size_t size;
	size_t capacity;
} StringBuffer;

// Funções de interface de linha de comando
void cliWaitUserCommand();
void cliHelp();

// Funções de execução do emulador
int emuExecute(uint8_t opcode, uint16_t argument);
void emuDoArit(uint16_t argument);
uint16_t* emuGetRegister(uint8_t code);
void emuFault(const char* fmt, ...);
void emuGuardAddress(uint16_t addr);
void emuBadInstruction();
void emuDumpRegisters();
StringBuffer emuDisassembly(uint16_t instruction);

// Funções auxiliares de manipulação de strings
void stbInit(StringBuffer*);
bool stbPrint(StringBuffer*, const char* fmt, ...);
void stbFree(StringBuffer*); 

// Funções auxiliares genéricas
void INTHandler(int sign);
void setBit(uint16_t* reg, int bit, bool value);
bool getBit(uint16_t value, int bit);
void toLowerCase(char* str);
void pause();

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
	"A",   // 000b
	"B",   // 001b
	"C",   // 010b
	"D",   // 011b
	"?",   // 100b
	"?",   // 101b
	"R",   // 110b
	"PSW", // 111b
};

static const char* const ARIT_OP_NAMES[] = {
	"SET0", // 000b
	"SETF", // 001b
	"NOT",  // 010b
	"AND",  // 011b
	"OR",   // 100b
	"XOR",  // 101b
	"ADD",  // 110b
	"SUB"   // 111b
};

// Estrutura global de emulação
static Emul emulator;
static time_t lastInterruptBreak;

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
	emulator.breaking = START_IN_BREAKING_MODE;
	emulator.stepsLeft = 0;

	time(&lastInterruptBreak);
	signal(SIGINT, INTHandler);

	printf("\n--- PROTO EMULATOR ---\n");
	printf("Memory size: 0x%X words.\n", memSize);
	printf("Press CTRL-C to break execution and start debugging.\n");

	printf("Beginning execution...\n");
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
		StringBuffer instructionStr = emuDisassembly(r.RI);
		printf("%s\n", instructionStr.buffer);
		stbFree(&instructionStr);

		// Se o emulador está em modo step-through, permita ao usuário decidir o que fazer antes
		// de executar qualquer instrução
		if (emulator.breaking) {
			cliWaitUserCommand();
		}

		// Executa a instrução com seu argumento opcional
		int result = emuExecute(opcode, argument);
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

void cliWaitUserCommand() {
	const size_t BUFFER_SIZE = 128;
	static char commandBuffer1[128] = { 0 };
	static char commandBuffer2[128] = { 0 };

	static char* commandBuffer = commandBuffer1;
	static char* lastCommand = commandBuffer2;

	// Se o usuário pediu para executar um número x de instruções antes, não pare a execução
	// nessa função
	if (emulator.stepsLeft > 0) {
		--emulator.stepsLeft;
		return;
	}

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

		if (cmdLine[0] == '\0') continue;

		// Obtém o comando principal antes do espaço
		char* command = strtok(cmdLine, " ");
		//toLowerCase(command);

		// Comando step <amount>: Permite executar um número de instruções em sequência
		if (strcmp(command, "s") == 0 || strcmp(command, "step") == 0) {
			emulator.stepsLeft = 0;

			char* amountStr = strtok(NULL, " ");
			if (amountStr) {
				sscanf(amountStr, "%i", &emulator.stepsLeft);
				emulator.stepsLeft--;
			}
			return;
		}

		// Comando continue: Desativa o modo step-through do emulador
		if (strcmp(command, "c") == 0 || strcmp(command, "continue") == 0) {
			printf("Resuming execution...\n");
			emulator.breaking = false;
			return;	
		}

		// Comando registers: Imprime o conteúdo de todos os registradores
		if (strcmp(command, "r") == 0 || strcmp(command, "registers") == 0) {
			emuDumpRegisters();
		}

		// Comando disassembly [address] [amount]: Imprime um número de instruções no endereço dado
		// Se não for passado um endereço. Imprime a instrução atual
		if (strcmp(command, "d") == 0 || strcmp(command, "disassembly") == 0) {
			uint32_t address = emulator.registers->PC;
			uint32_t amount = 1;

			// Se for passado um endereço, o transforma em número e salva em address
			char* addressStr = strtok(NULL, " ");
			if (addressStr) {
				sscanf(addressStr, "%X", &address);
			}

			// Garante que address está nos limites da memória
			if (address >= emulator.memorySize) {
				printf("Memory address 0x%X out of bounds (0x%X)\n", address, emulator.memorySize);
				continue;
			}

			// Se for passado uma quantidade, a transforma em número e salva em Amount
			char* amountStr = strtok(NULL, " ");
			if (amountStr) {
				sscanf(amountStr, "%X", &amount);
			}

			// Imprime as instruções linha por linha
			for (int i = 0; i < amount; i++) {
				uint32_t instruction = emulator.memory[address + i];
					
				uint8_t opcode = (instruction & 0xF000) >> 12;
				uint16_t argument = (instruction & 0x0FFF);

				StringBuffer sb = emuDisassembly(instruction);
				printf("[%3Xh] %X.%03X: %s\n", address + i, opcode, argument, sb.buffer);
				stbFree(&sb);
			}
		}

		// Comando memory <address> [words]: Observa a memória no ponto dado
		if (strcmp(command, "m") == 0 || strcmp(command, "x") == 0 || strcmp(command, "memory") == 0) {
			// Obtém em string o número do endereço
			char* pointStr = strtok(NULL, " ");
			if (!pointStr) {
				printf("A source point must be passed to the memory command.\n");
				continue;
			}

			// Transforma o endereço em número
			int point;
			sscanf(pointStr, "%X", &point);

			if (point < 0 || point >= emulator.memorySize) {
				printf("Memory address 0x%X out of bounds (0x%X)\n", point, emulator.memorySize);
				continue;
			}

			// Obtém o número de palavras a observar, se o usuário passar esse argumento
			int words = 8;
			char* wordsStr = strtok(NULL, " ");
			if (wordsStr) {
				sscanf(wordsStr, "%X", &words);
			}

			// Imprime as palavras
			for (int i = 0; i < words; i++) {
				if (i % 8 == 0) {
					printf("\n[%3Xh] ", point);
				}
				printf("%04X ", emulator.memory[point]);
				point++;
			}

			printf("\n");
		}

		if (strcmp(command, "help") == 0) {
			cliHelp();
		}
	}
}

void cliHelp() {
	printf("Pressing CTRL-C at any time will interrupt emulation.\nPressing it in quick succession will quit the emulator entirely.\n");
	printf("\nhelp: prints this help guide.\n");
	printf("\ncontinue, c:\n\tLeaves step-through mode and lets the emulator run freely.\n\tExecution will be stopped upon encountering a fault or the user\n\tpressing CTRL-C.\n");
	printf("\nregisters, r: \n\tView the contents of all CPU registers.\n");
	printf("\nstep, s <amount>:\n\tSteps through <amount> of instructions and no further.\n");
	printf("\nmemory, m, x <address> [words]:\n\tViews the contents of the emulator memory at the given address with an\n\toptional amount of words to display.\n");
	printf("\ndisassembly, d [address] [amount]:\n\tDisassembles the given amount of instructions at the address specified.\n\tIf no address is specified, prints the current instruction.\n");
	
}

int emuExecute(uint8_t opcode, uint16_t argument) {
	Registers* regs = emulator.registers;
	uint16_t* memory = emulator.memory;

	switch(opcode) {
	// NOP -- Opcode (0000)b
	// Não faz nada
	case 0x0:
		break;

	// LDA(x) -- Opcode (0001)b
	// Carrega o acumulador (A) com o conteúdo da memória em X
	case 0x1: {
		// Garante a validade do endereço de memória X
		emuGuardAddress(argument);

		regs->A = memory[argument];
		break;
	}

	// STA(x) -- Opcode (0010)b
	// Armazena no endereço imediato X o valor do acumulador A
	case 0x2: {
		// Garante a validade do endereço de memória X
		emuGuardAddress(argument);

		memory[argument] = regs->A;
		break;
	}

	// JMP(x) -- Opcode (0011)b
	// Pula incondicionalmente para o endereço imediato X
	case 0x3: {
		// Garante que o destino X de salto é válido
		emuGuardAddress(argument);

		// Salva R como o endereço da próxima instrução
		regs->R = regs->PC + 1;

		// Modifica o contador pro endereço X - 1. Subtraímos 1 pois o loop já incrementa
		// em 1 o PC.
		regs->PC = argument - 1;
		break;
	}

	// JNZ(x) -- Opcode (0100)b
	// Se o acumulador for != de 0, armazena o endereço da próxima instrução em R e salta para
	// o endereço especificado no argumento X
	case 0x4: {
		// Garante que o destino X de salto é válido
		emuGuardAddress(argument);

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
		emuDoArit(argument);
		break;
	}

	// HLT - Opcode (1111)b
	// Interrompe a execução do processador
	case 0xF:
		return 1;

	// Instrução desconhecida
	default: {
		emuBadInstruction();	
		break;
	}
	}

	return 0;
}

void emuDoArit(uint16_t argument) {
	Registers* regs = emulator.registers;
	uint16_t* PSW = &emulator.registers->PSW;

	// Extrai os 3 bits que determinam a operação aritmética a realizar
	uint8_t bitsOpr = (argument & 0b111000000000) >> 9;

	// Extrai os 3 bits que definem o registrador destino da operação
	uint8_t bitsDst = (argument & 0b000111000000) >> 6;

	// Extrai os bits que definem o registrador do primeiro operando
	uint8_t bitsOp1 = (argument & 0b000000111000) >> 3;

	// Extrai os bits que definem o registrador do segundo operando
	uint8_t bitsOp2 =  argument & 0b000000000111;

	// Obtém o registrador destino usando os bits de Res
	uint16_t* regDst = emuGetRegister(bitsDst);
	if (!regDst) {
		emuFault("Invalid arit register destination code: %i\n", bitsDst);
		return;
	}

	// Obtém o registrador operando usando os bits de Op1
	uint16_t* regOp1 = emuGetRegister(bitsOp1);
	if (!regOp1) {
		emuFault("Invalid arit register op1 code: %i\n", bitsOp1);
		return;
	}

	// Obtém o registrador operando usando os bits de Op2
	uint16_t* regOp2;

	// Se o bit mais significante do operando 2 for 0, o operando é considerado como o próprio número 0.
	if ((bitsOp2 & 0b100) == 0) {
		regOp2 = NULL;
	// Caso contrário, os outros dois bits selecionarão um registrador de A, B, C ou D
	} else {
		regOp2 = emuGetRegister(bitsOp2 & 0b011);
		if (!regOp2) {
			emuFault("Invalid arit register op2 code: %i\n", bitsOp2);
			return;
		}	
	}	

	// Obtém os valores dos registradores apontados por Op1 e Op2.
	// Novamente, se o registrador de Op2 for NULL, será considerado o valor imediato 0.
	uint16_t op1 = *regOp1;
	uint16_t op2 = (regOp2) ? *regOp2 : 0;

	switch(bitsOpr) {
	case 0:
		*regDst = 0x0000;
		break;
	case 1:
		*regDst = 0xFFFF;
		break;
	case 2:
		*regDst = ~op1;
		break;
	case 3:
		*regDst = op1 & op2;
		break;
	case 4:
		*regDst = op1 | op2;
		break;
	case 5:
		*regDst = op1 ^ op2;
		break;
	case 6:
		// Salva a soma já truncada no registrador
		uint32_t sum = op1 + op2;
		*regDst = (uint16_t)sum;

		// Seta o bit de overflow (bit 15) se a soma transborda ou não
		bool overflowed = sum > 0xFFFF;
		setBit(PSW, 15, overflowed);
		break;
	/*case 7:
		// Salva a subtração truncada no registrador
		*regDst = op1 - op2;

		// Seta o bit de underflow (bit 14) se a subtração transborda
		bool underflowed = op2 > op1;
		setBit(PSW, 14, overflowed);
		break;*/
	default:
		emuFault("Unimplemented arit operation %i\n", (int)bitsOpr);
		break;
	}

	// Compara os operandos 1 e 2 e seta os bits 13, 12 e 11 de acordo com as comparações
	bool less = op1 < op2;
	bool equal = op1 == op2;
	bool greater = op1 > op2;
	setBit(PSW, 13, less);
	setBit(PSW, 12, equal);
	setBit(PSW, 11, greater);
}

uint16_t* emuGetRegister(uint8_t code) {
	Registers* r = emulator.registers;
	switch (code) {
	case 0x0:
		return &r->A;
	case 0x1:
		return &r->B;
	case 0x2:
		return &r->C;
	case 0x3:
		return &r->D;
	case 0x6:
		return &r->R;
	case 0x7:
		return &r->PSW;
	default:
		return NULL;
	}
}

StringBuffer emuDisassembly(uint16_t instruction) {
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

	// JMP(x) -- 0011b
	case 0x3:
		stbPrint(buffer, "%Xh", argument);
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
		bool op2zero = (bitsOp2 & 0b100) == 0;

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
void emuGuardAddress(uint16_t addr) {
	if (addr >= emulator.memorySize) {
		emuFault("Memory access out of bounds (%x)\n", addr);
	}
}

void emuBadInstruction() {
	uint32_t RI = emulator.registers->RI;
	uint32_t PC = emulator.registers->PC;
	emuFault("Bad instruction 0x%X at 0x%X --\n", RI, PC);
}

void emuFault(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);

	printf("\n[!!] FAULT: ");
	vfprintf(stdout, fmt, args);

	// Coloca o emulador em modo step-through e interrompe qualquer sequência de steps se havia uma
	emulator.breaking = true;
	emulator.stepsLeft = 0;

	va_end(args);
}

void emuDumpRegisters() {
	Registers* regs = emulator.registers;
	printf("---- Program registers ----\n");
	uint16_t psw = regs->PSW;
	printf("PC:  0x%04hx\n", regs->PC);
	printf("RI:  0x%04hx\n", regs->RI);
	printf("PSW: 0x%04hx\n", regs->PSW);
	printf("  OV=%i UN=%i ", (int)getBit(psw, 15), (int)getBit(psw, 14));
	printf("LE=%i EQ=%i GR=%i\n", (int)getBit(psw, 13), (int)getBit(psw, 12), (int)getBit(psw, 11));
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

void INTHandler(int sign) {
	signal(sign, SIG_IGN);

	if (difftime(time(NULL), lastInterruptBreak) < 1.0) {
		exit(0);
	}

	time(&lastInterruptBreak);

	printf("\n-- Ctrl-C pressed. Breaking execution.\n");

	emulator.stepsLeft = 0;
	emulator.breaking = true;

	signal(SIGINT, INTHandler);
}

void setBit(uint16_t* reg, int bit, bool value) {
	uint16_t num = *reg;
	num &= ~(1UL << bit);            // Clear bit first
	num |= ((uint32_t)value) << bit; // Set bit
	*reg = num;
}

bool getBit(uint16_t value, int bit) {
	return (value >> bit) & 1UL;
}

void toLowerCase(char* str) {
	while (str) {
		*str = tolower(*str);
		str++;
	}
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
