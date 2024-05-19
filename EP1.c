/**
 * Emulador interativo para o processador protótipo de 16 bits do professor Fábio Nakano.
 * 
 * Esse programa visa ir além do exercício-programa 1 e implementa um emulador e depurador completo
 * para a arquitetura especificada pelo professor.
 * 
 * Autor: André Morales
 * Criação: 17/05/2024
 * Modificação: 19/05/204
 * */
#include "driverEP1.h"
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>

// Se configurado como 1, desativa todos os recursos interativos do emulador e o coloca em um modo
// de "apenas execução". Esse modo é usado em testes automatizados. 
#define FINAL_EX_MODE 0

// Configura se cores serão usadas ou não no console. Desative se estiver tendo problemas com
// terminais que não suportam escapes ANSI apropriadamente
#define ENABLE_COLORS 1

// Se habilitado, interrompe o emulador logo antes de executar a primeira instrução
#define START_IN_BREAKING_MODE 1

// Quando habilitado, intercepta CTRL-C para interromper a execução do emulador
#define INSTALL_SIGINT_HANDLER 1

// Se habilitado, por padrão interromperá a execução quando houver uma fault
#define BREAK_AT_FAULTS 1

// Configura se a notação extendida ou padrão será usada nos disassemblies
#define DEFAULT_EXTENDED_NOTATION 1

// Configura se a ocorrência de loop-around na memória gera uma fault ou apenas um aviso
#define FAULT_ON_LOOP_AROUND 1

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
	uint16_t* snapshot;
	int memorySize;
	bool breaking;
	int stepsLeft;
	bool breakOnFaults;
} Emul;

// Definição de string buffer para a formatação de mensagens
typedef struct {
	char* buffer;
	size_t position;
	size_t size;
	size_t capacity;
} StringBuffer;

// Guias de controle da interface de usuário
typedef enum {
	CLI_DO_NOTHING, CLI_DO_RESET, CLI_DO_QUIT
} CliControl;

// Resultados possíveis da execução de uma instrução
typedef enum {
	EMU_OK, EMU_HALT, EMU_FAULT
} EmuResult;

// -- Funções de interface de linha de comando
void cliPrintWelcome();
CliControl cliBeforeExecute();
CliControl cliWaitUserCommand();
void cliInstallIntHandler();
void cliStepCmd();
void cliContinueCmd();
void cliDisassemblyCmd();
void cliMemoryCmd();
void cliHelpCmd();
void signIntHandler(int sign);

// -- Funções de execução do emulador
void emuInitialize(uint16_t* memory, int memorySize);
void emuReset();
uint16_t emuFetch();
void emuAdvance();
EmuResult emuExecute(uint16_t instruction);
void emuDoArit(uint16_t argument);
uint16_t* emuGetRegister(uint8_t code);
void emuFault(const char* fmt, ...);
void emuWarn(const char* fmt, ...);
bool emuGuardAddress(uint16_t addr);
void emuBadInstruction();
void emuDumpRegisters();
void emuPrintDisassemblyLine(uint16_t address);
StringBuffer emuDisassembly(uint16_t instruction);

// -- Funções auxiliares de manipulação de strings
void stbInit(StringBuffer*);
bool stbPrint(StringBuffer*, const char* fmt, ...);
void stbFree(StringBuffer*); 

// -- Funções auxiliares genéricas
void setBit(uint16_t* reg, int bit, bool value);
bool getBit(uint16_t value, int bit);
void toLowerCase(char* str);
void pause();

// -- Sequências de escape para as cores no console
#if ENABLE_COLORS
#define TERM_BOLD_RED		"\033[1;31m"
#define TERM_BOLD_YELLOW	"\033[1;33m"
#define TERM_BOLD_MAGENTA	"\033[1;35m"
#define TERM_BOLD_CYAN		"\033[1;36m"
#define TERM_BOLD_WHITE		"\033[1;37m"
#define TERM_GREEN			"\033[32m"
#define TERM_YELLOW			"\033[33m"
#define TERM_CYAN			"\033[36m"
#define TERM_RESET			"\033[0m"
#else
#define TERM_BOLD_RED		""
#define TERM_BOLD_YELLOW	""
#define TERM_BOLD_MAGENTA	""
#define TERM_BOLD_CYAN		""
#define TERM_BOLD_WHITE		""
#define TERM_GREEN			""
#define TERM_YELLOW			""
#define TERM_CYAN			""
#define TERM_RESET			""
#endif

// Tabela com o nome das intruções para cada opcode
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

// Nome dos registradores
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

// Nome das operações aritméticas
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

// Formatação extendida para as operações aritméticas
static const char* const ARIT_EXT_FMT[] = {
	"%s = 0",		 // 000b
	"%s = FFFF",	 // 001b
	"%s = ~%s",		 // 010b
	"%s = %s & %s",  // 011b
	"%s = %s | %s",  // 100b
	"%s = %s ^ %s",  // 101b
	"%s = %s + %s",  // 110b
	"%s = %s - %s"   // 111b
};

// Estrutura globais de emulação
static Emul emulator;
static time_t lastInterruptBreak;
static bool extendedNotation = false;

// Entrada principal do programa. Essa função é chamada com um bloco de memória que corresponde ao
// estado inicial da memória do programa a ser emulado.
// O bloco de memória é válido durante toda a função principal.
int processa(short unsigned int* m, int memSize) {
	uint16_t* memory = (uint16_t*)m;

	// Imprime o cabeçalho de boas vindas
	cliPrintWelcome();
	
	// Configura um handler para o CTRL-C 
	cliInstallIntHandler();

	// Inicializa as estruturas do emulador
	emuInitialize(memory, memSize);
	
	printf("Memory size: 0x%X words.\n", memSize);
	printf("Beginning execution...\n");

	Registers* regs = emulator.registers;
	do {
		// Lê a instrução atual
		uint16_t instruction = emuFetch();

		// Imprime a posição, o opcode, argumento e disassembly da instrução atual
		emuPrintDisassemblyLine(regs->PC);

		// Antes de executar a instrução, interaja com o usuário na interface
		CliControl ctrl = cliBeforeExecute();

		// Se o usuário pediu um reset, vá para o início do loop
		if (ctrl == CLI_DO_RESET) continue;

		// Se o usuário pediu para sair do programa, saia do loop
		if (ctrl == CLI_DO_QUIT) break;

		// Executa a instrução
		EmuResult result = emuExecute(instruction);

		// Se a instrução era um HALT, sai do loop
		if (result == EMU_HALT) break;

		// Incrementa program counter para a próxima instrução
		emuAdvance();

	// Guarda adicional: O programa cessa ao encontrar HLT
	} while ((regs->RI & 0xF000) != 0xF000);

	printf("\nCPU Halted.\n");

	return 0;
}

// Imprime o cabeçalho de boas vindas
void cliPrintWelcome() {
	printf("\n---- PROTO EMULATOR 1.0 ----\n");
}

// Instala signIntHandler como um monitor para o CTRL-C
void cliInstallIntHandler() {
	#if !FINAL_EX_MODE && INSTALL_SIGINT_HANDLER
	printf("Press CTRL-C to break execution and start debugging.\n");
	time(&lastInterruptBreak);
	signal(SIGINT, signIntHandler);
	#endif
}

// Essa função é chamada sempre logo antes da execução efetiva de uma instrução.
// Se o emulador estiver em step-through, aqui haverá uma chamada para cliWaitUserCommand()
// para que o usuário interaja com o emulador
CliControl cliBeforeExecute() {
	// Se o usuário pediu para executar um número x de instruções antes (comando step),
	// não pare a execução nessa função
	if (emulator.stepsLeft > 0) {
		--emulator.stepsLeft;
		return CLI_DO_NOTHING;
	}

	// Se o emulador está em modo step-through, permita ao usuário decidir o que fazer antes
	// de efetivamente executar a instrução
	if (emulator.breaking) {
		CliControl ctrl = cliWaitUserCommand();
		return ctrl;		
	}

	return CLI_DO_NOTHING;
}

// Loop do prompt de comandos quando emulador estiver parado
CliControl cliWaitUserCommand() {
	const size_t BUFFER_SIZE = 128;
	static char commandBuffer1[128] = { 0 };
	static char commandBuffer2[128] = { 0 };
	static bool firstBreak = true;

	static char* commandBuffer = commandBuffer1;
	static char* lastCommand = commandBuffer2;

	// Se é a primeira vez que o usuário para a execução
	if (firstBreak) {
		firstBreak = false;
		printf(TERM_GREEN "You are in step-through mode. You can view memory contents, registers and disassembly.");
		printf("\nType " TERM_YELLOW "help" TERM_GREEN " to view all commands.\n" TERM_RESET);
	}

	// Loop infinito apenas interrompido quando o usuário digitar um comando válido
	while (true) {	
		// Lê uma linha de comando
		printf(TERM_BOLD_CYAN ">> " TERM_YELLOW);		
		fgets(commandBuffer, BUFFER_SIZE, stdin);
		printf("%s", TERM_RESET);

		// Remove a quebra de linha da string
		for (int i = 0; i < BUFFER_SIZE; i++) {
			if (commandBuffer[i] == '\n') {
				commandBuffer[i] = '\0';
				break;
			}
		}

		// Se a linha digitada for completamente vazia, o usuário apenas apertou enter e o comando
		// anterior deve ser executado novamente
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
		toLowerCase(command);

		// Comando step <amount>: Permite executar um número de instruções em sequência
		if (strcmp(command, "s") == 0 || strcmp(command, "step") == 0) {
			cliStepCmd();
			break;
		}

		// Comando continue: Desativa o modo step-through do emulador
		if (strcmp(command, "c") == 0 || strcmp(command, "continue") == 0) {
			cliContinueCmd();
			break;
		}

		// Comando registers: Imprime o conteúdo de todos os registradores
		if (strcmp(command, "r") == 0 || strcmp(command, "registers") == 0) {
			emuDumpRegisters();
			continue;
		}

		// Comando disassembly [address] [amount]: Imprime um número de instruções no endereço dado
		// Se não for passado um endereço. Imprime a instrução atual
		if (strcmp(command, "d") == 0 || strcmp(command, "disassembly") == 0) {
			cliDisassemblyCmd();
			continue;
		}

		// Comando memory <address> [words]: Observa a memória no ponto dado
		if (strcmp(command, "m") == 0 || strcmp(command, "x") == 0 || strcmp(command, "memory") == 0) {
			cliMemoryCmd();
			continue;
		}

		// Comando quit: Sai do emulador
		if (strcmp(command, "q") == 0 || strcmp(command, "quit") == 0) {
			return CLI_DO_QUIT;
		}

		// Comando reset: Reinicia o emulador com a memória original e os registradores em 0
		if (strcmp(command, "reset") == 0) {
			emuReset();
			return CLI_DO_RESET;
		}

		// Comando nobreak: Desabilita a parada do emulator no lançamento de falhas
		if (strcmp(command, "nobreak") == 0) {
			emulator.breakOnFaults = false;
			continue;
		}

		// Comando dobreak: Rehabilita a parada do emulator no lançamento de falhas
		if (strcmp(command, "dobreak") == 0) {
			emulator.breakOnFaults = true;
			continue;
		}

		// Comando help: Imprime a ajuda do programa
		if (strcmp(command, "help") == 0) {
			cliHelpCmd();
			continue;
		}

		printf(TERM_BOLD_RED "Unknown command '%s'. Type 'help' for a list of commands.\n" TERM_RESET, command);
	}

	return CLI_DO_NOTHING;
}

// Desabilita o modo step-through e deixa o emulador voltar a execução
void cliContinueCmd() {
	printf("Resuming execution...\n");
	emulator.breaking = false;
}

// Avança um certo número de passos na execução
void cliStepCmd() {
	emulator.stepsLeft = 0;

	char* amountStr = strtok(NULL, " ");
	if (amountStr) {
		sscanf(amountStr, "%i", &emulator.stepsLeft);
		emulator.stepsLeft--;
	}
}

// Imprime o disassembly das instruções desejadas
void cliDisassemblyCmd() {
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
		return;
	}

	// Se for passado uma quantidade, a transforma em número e salva em Amount
	char* amountStr = strtok(NULL, " ");
	if (amountStr) {
		sscanf(amountStr, "%X", &amount);
	}

	// Imprime as instruções linha por linha
	for (int i = 0; i < amount; i++) {
		emuPrintDisassemblyLine(address + i);
	}
}

// Exibe os conteúdos da posição de memória escolhida
void cliMemoryCmd() {
	// Obtém em string o número do endereço
	char* pointStr = strtok(NULL, " ");
	if (!pointStr) {
		printf("A source point must be passed to the memory command.\n");
		return;
	}

	// Transforma o endereço em número
	int point;
	sscanf(pointStr, "%x", &point);
	if (point < 0) {
		printf(TERM_BOLD_RED "Address must not be negative.\n" TERM_RESET);
		return;
	}

	// Obtém o número de palavras a observar, se o usuário passar esse argumento
	int words = 8;
	char* wordsStr = strtok(NULL, " ");
	if (wordsStr) {
		sscanf(wordsStr, "%X", &words);
	}

	// Imprime as palavras
	for (uint16_t i = 0; i < words; i++) {
		uint16_t addr = point + i;
		if (addr >= emulator.memorySize) {
			printf(TERM_BOLD_RED "Memory address 0x%X out of bounds (0x%X)\n" TERM_RESET, addr, emulator.memorySize);
			return;
		}

		if (i % 8 == 0) {
			printf(TERM_BOLD_WHITE "\n[%3Xh] " TERM_RESET, addr);
		}
		printf("%04X ", emulator.memory[addr]);
		addr++;
	}

	printf("\n");
}

// Imprime a mensagem de ajuda do emulador
void cliHelpCmd() {
	printf("Pressing CTRL-C at any time will interrupt emulation.\nPressing it in quick succession will quit the emulator entirely.\n");
	printf("\nhelp: prints this help guide.\n");
	printf("\nquit, q: quits out of the emulator.\n");
	printf("\nstep, s <amount>:\n    Steps through <amount> of instructions and no further.\n");
	printf("\ncontinue, c:\n    Leaves step-through mode and lets the emulator run freely.\n    Execution will be stopped upon encountering a fault or the user\n    pressing CTRL-C.\n");
	printf("\nreset:\n    Resets the memory state as it were in the beginning of the emulation\n    and clears all registers.\n");
	printf("\nregisters, r: \n    View the contents of all CPU registers.\n");
	printf("\nmemory, m, x <address> [words]:\n    Views the contents of the emulator memory at the given address with an\n    optional amount of words to display.\n");
	printf("\ndisassembly, d [address] [amount]:\n    Disassembles the given amount of instructions at the address specified.\n    If no address is specified, prints the current instruction.\n");
	printf("\nnobreak: disables emulator pauses on cpu faults.\n");
	printf("\ndobreak: reenables emulator pauses on cpu faults.\n");
}

// Inicializa o emulador com a memória dada. A memória é considerada viva e será modificada durante
// a execução do emulador.
void emuInitialize(uint16_t* memory, int memSize) {
	// Estrutura do emulador
	emulator.memory = memory;
	emulator.memorySize = memSize;
	emulator.registers = (Registers*)malloc(sizeof(Registers));
	emulator.stepsLeft = 0;
	emulator.breaking = false;
	emulator.breakOnFaults = false;

	// Salva uma cópia da memória passada em um "snapshot". Esse snapshot é utilizado caso
	// o usuário realize um 'reset' no emulador
	emulator.snapshot = (uint16_t*)malloc(memSize * sizeof(uint16_t));
	memcpy(emulator.snapshot, memory, memSize * sizeof(uint16_t));

	// Se configurado para tal, começa o emulador já no modo step-through
	#if !FINAL_EX_MODE && START_IN_BREAKING_MODE
	emulator.breaking = true;
	#endif

	// Habilita a notação extendida por padrão
	#if !FINAL_EX_MODE && DEFAULT_EXTENDED_NOTATION
	extendedNotation = true;
	#endif

	// Se configurado como tal pelas flags, para o emulador se alguma fault for lançada
	#if !FINAL_EX_MODE && BREAK_AT_FAULTS
	emulator.breakOnFaults = true;
	#endif

	emuReset();
}

// Realiza um reset. Todos os registradores são reinicializados para 0 e a memória viva é
// reinicializada com uma cópia do snapshot feito na inicialização
void emuReset() {
	// Inicializa para 0 todos os registradores
	Registers* regs = emulator.registers;
	regs->RI = 0;
	regs->PC = 0;
	regs->A = 0;
	regs->B = 0;
	regs->C = 0;
	regs->D = 0;
	regs->R = 0;
	regs->PSW = 0;

	// Copia a memória inicial do programa para a memória principal
	memcpy(emulator.memory, emulator.snapshot, emulator.memorySize * 2);
}

// Obtém a instrução atual apontada pelo program counter. Atualiza o registrador de instruções RI
uint16_t emuFetch() {
	Registers* regs = emulator.registers;

	// Lê da memória o valor em Program Counter e salva no registrador de instrução atual
	uint16_t instruction = emulator.memory[regs->PC];
	regs->RI = instruction;	
	return instruction;
}

// Avança o program counter uma instrução a frente
void emuAdvance() {
	Registers* regs = emulator.registers;

	// Incrementa o ponteiro para a próxima instrução
	regs->PC++;

	// Se o contador de programa ultrapassou o limite da memória, reinicie-o em 0.
	if (regs->PC >= emulator.memorySize) { 
		#if FAULT_ON_LOOP_AROUND
		emuFault("Program counter looped around to 0. Was program control lost?");
		#else
		emuWarn("Program counter looped around to 0. Was program control lost?");
		#endif

		regs->PC = 0;
	}
}

// Executa a instrução dada como parâmetro
EmuResult emuExecute(uint16_t instruction) {
	Registers* regs = emulator.registers;
	uint16_t* memory = emulator.memory;

	// Extrai da instrução os 4 bits do código de operação
	// e os bits do argumento X da instrução
	uint8_t opcode =    (instruction & 0xF000) >> 12;
	uint16_t argument = (instruction & 0x0FFF);

	switch(opcode) {
	// NOP -- Opcode (0000)b
	// Não faz nada
	case 0x0:
		break;

	// LDA(x) -- Opcode (0001)b
	// Carrega o acumulador (A) com o conteúdo da memória em X
	case 0x1: {
		// Garante a validade do endereço de memória X
		if (emuGuardAddress(argument)) return EMU_FAULT;

		regs->A = memory[argument];
		break;
	}

	// STA(x) -- Opcode (0010)b
	// Armazena no endereço imediato X o valor do acumulador A
	case 0x2: {
		// Garante a validade do endereço de memória X
		if (emuGuardAddress(argument)) return EMU_FAULT;

		memory[argument] = regs->A;
		break;
	}

	// JMP(x) -- Opcode (0011)b
	// Pula incondicionalmente para o endereço imediato X
	case 0x3: {
		// Garante que o destino X de salto é válido
		if (emuGuardAddress(argument)) return EMU_FAULT;

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
		if (emuGuardAddress(argument)) return EMU_FAULT;

		if (regs->A != 0) {
			// Salva R como o endereço da próxima instrução
			regs->R = regs->PC + 1;

			// Modifica o contador pro endereço X - 1. Subtraímos 1 pois o loop já incrementa
			// em 1 o PC.
			regs->PC = argument - 1;
		}

		break;
	}

	// RET -- Opcode (0101b)
	case 0x5: {
		// Garante que o endereço de retorno será válido
		if (emuGuardAddress(regs->R)) return EMU_FAULT;

		// Salva o contador de programa atual. O contador passará a ser o endereço em R,
		// e R passará a ser o endereço da instrução depois dessa
		uint16_t pc = regs->PC;
		regs->PC = regs->R;
		regs->R = pc + 1;
		break;
	}

	// ARIT(x) -- Opcode (0110)b
	// Executa uma operação aritmética.
	case 0x6:
		emuDoArit(argument);
		break;

	// HLT - Opcode (1111)b
	// Interrompe a execução do processador
	case 0xF:
		return EMU_HALT;

	// Instrução desconhecida
	default:
		emuBadInstruction();	
		return EMU_FAULT;
	}

	return EMU_OK;
}

// Executa uma instrução de aritmética ARIT com o argumento X completo
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
	case 7:
		// Salva a subtração truncada no registrador
		*regDst = op1 - op2;

		// Seta o bit de underflow (bit 14) se a subtração transborda
		bool underflowed = op2 > op1;
		setBit(PSW, 14, underflowed);
		break;
	default:
		emuFault("Unimplemented arit operation %i\n", (int)bitsOpr);
		return;
	}

	// Compara os operandos 1 e 2 e seta os bits 13, 12 e 11 de acordo com as comparações
	bool less = op1 < op2;
	bool equal = op1 == op2;
	bool greater = op1 > op2;
	setBit(PSW, 13, less);
	setBit(PSW, 12, equal);
	setBit(PSW, 11, greater);
}

// Retorna o endereço do registrador correspondente ao código de 3 bits passado.
// Os registradores são { A, B, C, D, _, _, R, PSW }
// Se um código inválido for passado, NULL é retornado
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
	}
	return NULL;
}

// Imprime no console uma linha com o endereço e disassembly da instrução apontada pelo
// endereço passado como argumento
void emuPrintDisassemblyLine(uint16_t address) {
	// Obtém a instrução no endereço
	uint32_t instruction = emulator.memory[address];
					
	// Extrai da instrução os bits do código de operação e argumento X
	uint8_t opcode = (instruction & 0xF000) >> 12;
	uint16_t argument = (instruction & 0x0FFF);

	// Transforma a instrução em string
	StringBuffer sb = emuDisassembly(instruction);
	printf(TERM_BOLD_WHITE "[%3Xh] " TERM_RESET "%X.%03X: " TERM_CYAN "%s" TERM_RESET "\n", address, opcode, argument, sb.buffer);
	stbFree(&sb);
}

// Retorna uma string que representa o disassembly da instrução passada
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

		if (extendedNotation) {
			// Imprime RES = OP1 * OP2
			stbPrint(buffer, ARIT_EXT_FMT[bitsOpr],
				REGISTER_NAMES[bitsDst],
				REGISTER_NAMES[bitsOp1],
				(op2zero) ? "0" : REGISTER_NAMES[bitsOp2 & 0b011]
			);
		} else {
			// Imprime em sequência OPERAÇÃO, RES, OP1, OP2
			stbPrint(buffer, "%s, ", ARIT_OP_NAMES[bitsOpr]);
			stbPrint(buffer, "%s, ", REGISTER_NAMES[bitsDst]);
			stbPrint(buffer, "%s, ", REGISTER_NAMES[bitsOp1]);
			stbPrint(buffer, "%s ", (op2zero) ? "zero" : REGISTER_NAMES[bitsOp2 & 0b011]);
		}
		break;

	// HLT - 1111b
	case 0xF:
		break;

	// Instrução desconhecida
	default:
		stbPrint(buffer, ":: %X.%03X", opcode, argument);
		break;
	}

	return bufferStg;
}

// Verifica se um enderço se memória está dentro dos limites possíveis do tamanho da memória
// do emulador. Se o endereço estiver fora do limite, causa uma falha e retorna true.
bool emuGuardAddress(uint16_t addr) {
	if (addr >= emulator.memorySize) {
		uint32_t PC = emulator.registers->PC;

		emuFault("Memory access out of bounds 0x%04X at 0x%03X", addr, PC);
		return true;
	}

	return false;
}

// Lança uma falha de CPU por tentar executar uma instrução mal formulada ou que não existe
void emuBadInstruction() {
	uint32_t RI = emulator.registers->RI;
	uint32_t PC = emulator.registers->PC;
	emuFault("Bad instruction 0x%04X at 0x%03X", RI, PC);
}

// Lança uma falha de CPU com a mensagem formata desejada
void emuFault(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);

	printf(TERM_BOLD_RED "[ERR!] CPU FAULT: " TERM_RESET);
	vfprintf(stdout, fmt, args);
	printf("\n\n");

	// Coloca o emulador em modo step-through e interrompe qualquer sequência de steps se havia
	// alguma antes
	if (emulator.breakOnFaults) {
		emulator.breaking = true;
		emulator.stepsLeft = 0;
	}

	va_end(args);
}

// Imprime no console um warning
void emuWarn(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);

	printf(TERM_BOLD_YELLOW "[WRN!] " TERM_RESET);
	vfprintf(stdout, fmt, args);
	printf("\n\n");

	va_end(args);
}

// Imprime no terminal o conteúdo de todos os registradores da CPU emulada
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

// Handler de SIGINT (interrupção pelo CTRL-C)
void signIntHandler(int sign) {
	static bool interruptedBefore = false;

	// Remove all console colors
	printf(TERM_RESET " ");

	// Retransmit signal
	signal(sign, SIG_IGN);

	// If ctrl-c was pressed before and within a certain time threshold, exit the application
	if (difftime(time(NULL), lastInterruptBreak) < 1.5 && interruptedBefore) {
		exit(0);
	}

	// Save last time ctrl-c was pressed
	interruptedBefore = true;
	time(&lastInterruptBreak);

	printf("\n-- Ctrl-C pressed. Breaking execution.\n");

	// Put the emulator in breaking mode
	emulator.stepsLeft = 0;
	emulator.breaking = true;

	// Reset signal handler
	signal(SIGINT, signIntHandler);
}

// -- Funções auxiliares genéricas --
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
	while (*str) {
		*str = tolower(*str);
		str++;
	}
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
