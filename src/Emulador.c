/**
 * Emulador interativo para o processador protótipo de 16 bits do professor Fábio Nakano.
 * 
 * Esse programa visa ir além do exercício-programa 1 e implementa um emulador e depurador completo
 * para a arquitetura especificada pelo professor.
 * 
 * Autor: André Morales
 * Criação: 17/05/2024
 * Modificação: 04/06/2024
 **/

// Se configurado como 1, desativa todos os recursos interativos do emulador e o coloca em um modo
// de "apenas execução". Esse modo é usado em testes automatizados. 
#define DUMMY_MODE 0

// Configura se cores serão usadas ou não no console. Desative se estiver tendo problemas com
// terminais que não suportam escapes ANSI apropriadamente
#define ENABLE_COLORS 1

// Se habilitado, interrompe o emulador logo antes de executar a primeira instrução
#define START_IN_BREAKING_MODE 1

// Quando habilitado, intercepta CTRL-C para interromper a execução do emulador
#define INSTALL_SIGINT_HANDLER 1

// Se habilitado, por padrão interromperá a execução quando houver uma fault
#define BREAK_AT_FAULTS 1

// Se habilitado, antes de terminar a execução do programa, interrompe uma última vez o depurador
#define BREAK_AT_HALT 1

// Configura se a notação extendida ou padrão será usada nos disassemblies
#define DEFAULT_EXTENDED_NOTATION 1

// Configura se a ocorrência de loop-around na memória gera uma fault ou apenas um aviso
#define FAULT_ON_LOOP_AROUND 1

#include "driverEP1.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>

// -- Sequências de escape para as cores no console
#if ENABLE_COLORS
#define TERM_BOLD_BLACK		"\033[1;30m"
#define TERM_BOLD_RED		"\033[1;31m"
#define TERM_BOLD_GREEN		"\033[1;32m"
#define TERM_BOLD_YELLOW	"\033[1;33m"
#define TERM_BOLD_MAGENTA	"\033[1;35m"
#define TERM_BOLD_CYAN		"\033[1;36m"
#define TERM_BOLD_WHITE		"\033[1;37m"
#define TERM_RED			"\033[0;31m"
#define TERM_GREEN			"\033[32m"
#define TERM_YELLOW			"\033[33m"
#define TERM_MAGENTA		"\033[0;35m"
#define TERM_CYAN			"\033[0;36m"
#define TERM_WHITE			"\033[37m"
#define TERM_RESET			"\033[0m"
#else
#define TERM_BOLD_BLACK		""
#define TERM_BOLD_RED		""
#define TERM_BOLD_GREEN		""
#define TERM_BOLD_YELLOW	""
#define TERM_BOLD_MAGENTA	""
#define TERM_BOLD_CYAN		""
#define TERM_BOLD_WHITE		""
#define TERM_RED			""
#define TERM_GREEN			""
#define TERM_YELLOW			""
#define TERM_MAGENTA		""
#define TERM_CYAN			""
#define TERM_RESET			""
#endif

/// @brief Opcodes de 4 bits de todas as instruções do processador.
typedef enum {
	OPCODE_NOP  = 0b0000,
	OPCODE_LDA  = 0b0001,
	OPCODE_STA  = 0b0010,
	OPCODE_JMP  = 0b0011,
	OPCODE_JNZ  = 0b0100,
	OPCODE_RET  = 0b0101,
	OPCODE_ARIT = 0b0110,
	OPCODE_HLT  = 0b1111
} Opcode;

typedef enum {
	ARIT_SET0 = 0b000,
	ARIT_SETF = 0b001,
	ARIT_NOT  = 0b010,
	ARIT_AND  = 0b011,
	ARIT_OR   = 0b100,
	ARIT_XOR  = 0b101,
	ARIT_ADD  = 0b110,
	ARIT_SUB  = 0b111
} AritOp;

/// @brief Vetor dinâmico de inteiros
typedef struct VectorT {
	void** array;
	int size;
	int capacity;
} Vector;

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

/// @brief Estrutura de um breakpoint na memória. Possui um endereço e um máximo de hits.
typedef struct BreakpointT {
	uint16_t address;
	int hits;
} Breakpoint;

/// @brief Definição da estrutura do emulador
typedef struct {
	Registers* registers;
	uint16_t* memory;
	uint16_t* snapshot;
	int memorySize;
	bool breaking;
	int stepsLeft;
	bool breakOnFaults;
	Vector breakpoints;
} Emul;

// Guias de controle da interface de usuário
typedef enum {
	CLI_DO_NOTHING, CLI_DO_RESET, CLI_DO_QUIT
} CliControl;

// Resultados possíveis da execução de uma instrução
typedef enum {
	EMU_OK, EMU_HALT, EMU_FAULT
} EmuResult;


/// @brief Permite a manipulação e concatenação de strings formatadas
typedef struct StringBufferT {
	char* array;
	size_t size;
	size_t capacity;
} StringBuffer;


// -- Funções de manipulação de vetor dinâmico

void vecInit(Vector* vec);
void vecFree(Vector* vec);
void vecGrow(Vector* vec);
void vecAdd(Vector* vec, void* elem);
void vecRemove(Vector* vec, int index);
void prints(const char* fmt, ...);

// -- Funções auxiliares genéricas

bool strEquals(const char* a, const char* b);
void setBit(uint16_t* reg, int bit, bool value);
bool getBit(uint16_t value, int bit);
void toLowerCase(char* str);

// -- Funções auxiliares de manipulação de strings

void stbInit(StringBuffer*);
void stbGrow(StringBuffer* sb, size_t minRequiredSize);
bool stbAppend(StringBuffer*, const char* fmt, ...);
bool stbAppendv(StringBuffer* sb, const char* fmt, va_list args);
void stbAppendBuffer(StringBuffer* sb, StringBuffer* buffer);
void stbColorize(StringBuffer* sb, bool outputColors);
void stbFree(StringBuffer*);

// -- Funções de interface de linha de comando

void cliPrintWelcome();
CliControl cliBeforeExecute();
void emuCheckBreakpoints();
CliControl cliWaitUserCommand();
void cliInstallIntHandler();
void cliStepCmd();
void cliBreakpointCmd();
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
void emuSetBreakpoint(uint16_t addr, int hits);
Breakpoint * emuGetBreakpoint(uint16_t addr);
bool emuGuardAddress(uint16_t addr);
void emuBadInstruction();
void emuDumpRegisters();
void emuPrintDisassemblyLine(uint16_t address);
StringBuffer emuDisassembly(uint16_t instruction);

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
bool terminalColorsEnabled = ENABLE_COLORS;

/// @brief Entrada principal do programa. Essa função é chamada com um bloco de memória que corresponde ao
/// estado inicial da memória do programa a ser emulado.
/// O bloco de memória é válido durante toda a função principal.
int processa(short int* m, int memSize) {
	uint16_t* memory = (uint16_t*)m;

	// Imprime o cabeçalho de boas vindas
	cliPrintWelcome();
	
	// Configura um handler para o CTRL-C 
	cliInstallIntHandler();

	// Inicializa as estruturas do emulador
	emuInitialize(memory, memSize);

	printf("Memory size: 0x%X words.\n", memSize);
	printf("Beginning execution...\n\n");

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
	printf(TERM_CYAN "\n---- PROTO EMULATOR V1.1a ----\n");
	printf("GitHub: " TERM_BOLD_MAGENTA "https://github.com/andre-morales/emulador-oac\n\n" TERM_RESET);
}

// Instala signIntHandler como um monitor para o CTRL-C
void cliInstallIntHandler() {
	#if !DUMMY_MODE && INSTALL_SIGINT_HANDLER
	printf("Press CTRL-C to break execution and start debugging.\n");
	time(&lastInterruptBreak);
	signal(SIGINT, signIntHandler);
	#endif
}

// Essa função é chamada sempre logo antes da execução efetiva de uma instrução.
// Se o emulador estiver em step-through, aqui haverá uma chamada para cliWaitUserCommand()
// para que o usuário interaja com o emulador
CliControl cliBeforeExecute() {
	// Verifica e para em breakpoints nessa instrução se houverem
	emuCheckBreakpoints();

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

/// @brief Verifica se há um breakpoint válido na instrução atual.
/// Caso houver, pare a execução do emulador e imprime uma mensagem na tela.
void emuCheckBreakpoints() {
	// Obtém o breakpoint declarado nessa posição de memória
	uint16_t PC = emulator.registers->PC;
	Breakpoint* bp = emuGetBreakpoint(PC);

	// Se houver o breakpoint e ele ainda estiver ativo
	if (bp && bp->hits != 0) {
		// Coloca o emulador em modo step-through
		emulator.stepsLeft = 0;
		emulator.breaking = true;

		if (bp->hits > 0) bp->hits--;
		printf(TERM_GREEN "You've hit a breakpoint at " TERM_YELLOW "0x%03X.\n" TERM_RESET, PC);
		
		if (bp->hits > 0) {
			printf(TERM_GREEN "This breakpoint has" TERM_YELLOW " %i " TERM_GREEN "hits left.\n" TERM_RESET, bp->hits);
		} else if (bp->hits == 0) {
			printf(TERM_GREEN "This breakpoint was disabled.\n" TERM_RESET);
		}
	} else {
		#if BREAK_AT_HALT && !DUMMY_MODE
			uint16_t opcode = (emulator.registers->RI & 0xF000) >> 12;
			if (opcode == OPCODE_HLT) {
				// Coloca o emulador em modo step-through
				emulator.stepsLeft = 0;
				emulator.breaking = true;
			}
		#endif
	}
}

// Loop do prompt de comandos quando emulador estiver parado
CliControl cliWaitUserCommand() {
#define _COMMAND_BUFFER_SIZE 128
	const size_t BUFFER_SIZE = _COMMAND_BUFFER_SIZE;
	static char commandBuffer1[128] = { 0 };
	static char commandBuffer2[128] = { 0 };
	static bool firstBreak = true;

	static char* commandBuffer = commandBuffer1;
	static char* lastCommand = commandBuffer2;

	// Se é a primeira vez que o usuário para a execução
	if (firstBreak) {
		firstBreak = false;
		printf(TERM_GREEN "You are in step-through mode. ");
		printf("You can view memory contents, registers and disassembly.\n");
		printf("Type " TERM_YELLOW "help" TERM_GREEN " to view all commands.\n" TERM_RESET);
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
		char cmdLine[_COMMAND_BUFFER_SIZE];
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
		char* cmd = strtok(cmdLine, " ");
		toLowerCase(cmd);

		// Comando step <amount>: Permite executar um número de instruções em sequência
		if (strEquals(cmd, "s") || strEquals(cmd, "step")) {
			cliStepCmd();
			break;
		}

		// Comando continue: Desativa o modo step-through do emulador
		if (strEquals(cmd, "c") || strEquals(cmd, "continue")) {
			cliContinueCmd();
			break;
		}

		// Comando registers: Imprime o conteúdo de todos os registradores
		if (strEquals(cmd, "r") || strEquals(cmd, "regs") || strEquals(cmd, "registers")) {
			emuDumpRegisters();
			continue;
		}

		// Comando disassembly [address] [amount]: Imprime um número de instruções no endereço dado
		// Se não for passado um endereço. Imprime a instrução atual
		if (strEquals(cmd, "d") || strEquals(cmd, "disassembly")) {
			cliDisassemblyCmd();
			continue;
		}

		// Comando memory <address> [words]: Observa a memória no ponto dado
		if (strEquals(cmd, "m") || strEquals(cmd, "x") || strEquals(cmd, "memory")) {
			cliMemoryCmd();
			continue;
		}

		// Comando break <address> [hits]
		if (strEquals(cmd, "b") || strEquals(cmd, "break")) {
			cliBreakpointCmd();
			continue;
		}

		// Comando quit: Sai do emulador
		if (strEquals(cmd, "q") || strEquals(cmd, "quit")) {
			return CLI_DO_QUIT;
		}

		// Comando reset: Reinicia o emulador com a memória original e os registradores em 0
		if (strEquals(cmd, "reset")) {
			printf("Reseting all registers and memory.");
			emuReset();
			printf(" Done.\n");
			return CLI_DO_RESET;
		}

		// Comando nobreak: Desabilita a parada do emulator no lançamento de falhas
		if (strEquals(cmd, "nobreak")) {
			emulator.breakOnFaults = false;
			continue;
		}

		// Comando dobreak: Rehabilita a parada do emulator no lançamento de falhas
		if (strEquals(cmd, "dobreak")) {
			emulator.breakOnFaults = true;
			continue;
		}

		// Comando help: Imprime a ajuda do programa
		if (strEquals(cmd, "help")) {
			cliHelpCmd();
			continue;
		}

		printf(TERM_BOLD_RED "Unknown command '%s'. ", cmd);
		printf("Type 'help' for a list of commands.\n" TERM_RESET);
	}

	return CLI_DO_NOTHING;
}

// Desabilita o modo step-through e deixa o emulador voltar a execução
void cliContinueCmd() {
	printf(TERM_GREEN "Resuming execution...\n" TERM_RESET);
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

/// @brief Comando break [address] [hits] do emulador
void cliBreakpointCmd() {
	char* addressStr = strtok(NULL, " ");
	char* hitsStr = strtok(NULL, " ");

	int address = emulator.registers->PC;
	int hits = -1;

	if (addressStr) {
		sscanf(addressStr, "%x", &address);
	}
	
	if (hitsStr) {
		sscanf(hitsStr, "%i", &hits);
	}

	if (address < 0 || address >= emulator.memorySize) {
		printf(TERM_BOLD_RED "Address out of bounds.\n" TERM_RESET);
	}

	emuSetBreakpoint(address, hits);

	printf(TERM_GREEN "Breakpoint set at" TERM_YELLOW " 0x%03X.\n" TERM_RESET, address);
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
		uint16_t addr = address + i;

		// Previne a leitura de endereços inválidos
		if (addr >= emulator.memorySize) {
			printf(TERM_BOLD_RED "Instruction address 0x%X out of bounds (0x%X)\n" TERM_RESET, addr, emulator.memorySize);
			return;
		}

		emuPrintDisassemblyLine(addr);
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
	printf("Pressing " TERM_BOLD_RED "CTRL-C" TERM_RESET " at any time will interrupt emulation.");
	printf("\nPressing it in quick succession will " TERM_BOLD_RED "quit" TERM_RESET " the emulator entirely.\n");
	printf(TERM_CYAN  "\nhelp:" TERM_RESET " prints this help guide.\n");
	printf(TERM_CYAN  "\nquit, q:" TERM_RESET " quits out of the emulator.\n");
	prints("\n§6step, s§E [amount]§R");
	prints("\n    Steps through§E amount§R of instructions and no further.\n");
	prints("    If no amount is specified, steps a single instruction.\n");
	printf(TERM_CYAN  "\ncontinue, c");
	printf(TERM_RESET "\n    Leaves step-through mode and lets the emulator run freely.\n    Execution will be stopped upon encountering a fault or the user\n    pressing CTRL-C.\n");
	printf(TERM_CYAN  "\nreset");
	printf(TERM_RESET "\n    Resets the memory state as it were in the beginning of the emulation\n    and clears all registers.\n");
	prints("\n§6break, b§E [address] [hits]§R");
	prints("\n    Sets or unsets a breakpoint at a memory§E address§R.\n    If no address is specified, the breakpoint will be set at the current location.\n    The optional§E hits§R parameter causes the breakpoint to be disabled\n    automatically after being hit the specified amount of times.\n");
	printf(TERM_CYAN  "\nregisters, regs, r");
	printf(TERM_RESET "\n    View the contents of all CPU registers.\n");
	prints("\n§6memory, m, x§E <address> [words]§R");
	prints("\n    Views the contents of the emulator memory at the given§E address§R with an\n    optional amount of§E words§R to display.\n");
	prints("\n§6disassembly, d§E [address] [amount]§R");
	prints("\n    Disassembles the given§E amount§R of instructions at the§E address§R specified.\n    If no address is specified, prints the current instruction.\n");
	printf(TERM_CYAN  "\nnobreak:" TERM_RESET " disables emulator pauses on cpu faults.\n");
	printf(TERM_CYAN  "\ndobreak:" TERM_RESET " reenables emulator pauses on cpu faults.\n");
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
	vecInit(&emulator.breakpoints);

	// Salva uma cópia da memória passada em um "snapshot". Esse snapshot é utilizado caso
	// o usuário realize um 'reset' no emulador
	emulator.snapshot = (uint16_t*)malloc(memSize * sizeof(uint16_t));
	memcpy(emulator.snapshot, memory, memSize * sizeof(uint16_t));

	// Se configurado para tal, começa o emulador já no modo step-through
	#if !DUMMY_MODE && START_IN_BREAKING_MODE
	emulator.breaking = true;
	#endif

	// Habilita a notação extendida por padrão
	#if !DUMMY_MODE && DEFAULT_EXTENDED_NOTATION
	extendedNotation = true;
	#endif

	// Se configurado como tal pelas flags, para o emulador se alguma fault for lançada
	#if !DUMMY_MODE && BREAK_AT_FAULTS
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
	uint8_t opcodeBits = (instruction & 0xF000) >> 12;
	uint16_t argument =  (instruction & 0x0FFF);

	Opcode opcode = (Opcode)opcodeBits;
	switch(opcode) {
	// Não faz nada
	case OPCODE_NOP:
		break;

	// Carrega o acumulador (A) com o conteúdo da memória em X
	case OPCODE_LDA: {
		// Garante a validade do endereço de memória X
		if (emuGuardAddress(argument)) return EMU_FAULT;

		regs->A = memory[argument];
		break;
	}

	// Armazena no endereço imediato X o valor do acumulador A
	case OPCODE_STA: {
		// Garante a validade do endereço de memória X
		if (emuGuardAddress(argument)) return EMU_FAULT;

		memory[argument] = regs->A;
		break;
	}

	// Pula incondicionalmente para o endereço imediato X
	case OPCODE_JMP: {
		// Garante que o destino X de salto é válido
		if (emuGuardAddress(argument)) return EMU_FAULT;

		// Salva R como o endereço da próxima instrução
		regs->R = regs->PC + 1;

		// Modifica o contador pro endereço X - 1. Subtraímos 1 pois o loop já incrementa
		// em 1 o PC.
		regs->PC = argument - 1;
		break;
	}

	// Se o acumulador for != de 0, armazena o endereço da próxima instrução em R e salta para
	// o endereço especificado no argumento X
	case OPCODE_JNZ: {
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

	// Salva em R um endereço de retorno e pula para o conteúdo de R anterior.
	case OPCODE_RET: {
		// Garante que o endereço de retorno será válido
		if (emuGuardAddress(regs->R)) return EMU_FAULT;

		// Salva o contador de programa atual. O contador passará a ser o endereço em R,
		// e R passará a ser o endereço da instrução depois dessa
		uint16_t pc = regs->PC;
		regs->PC = regs->R - 1;
		regs->R = pc + 1;
		break;
	}

	// Executa uma operação aritmética.
	case OPCODE_ARIT:
		emuDoArit(argument);
		break;

	// Interrompe a execução do processador
	case OPCODE_HLT:
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

	// Se o bit mais significante do operando 2 for 0,
	// o operando é considerado como o próprio número 0.
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
	case ARIT_SET0:
		*regDst = 0x0000;
		break;
	case ARIT_SETF:
		*regDst = 0xFFFF;
		break;
	case ARIT_NOT:
		*regDst = ~op1;
		break;
	case ARIT_AND:
		*regDst = op1 & op2;
		break;
	case ARIT_OR:
		*regDst = op1 | op2;
		break;
	case ARIT_XOR:
		*regDst = op1 ^ op2;
		break;
	case ARIT_ADD:
		// Salva a soma já truncada no registrador
		uint32_t sum = op1 + op2;
		*regDst = (uint16_t)sum;

		// Seta o bit de overflow (bit 15) se a soma transborda ou não
		bool overflowed = sum > 0xFFFF;
		setBit(PSW, 15, overflowed);
		break;
	case ARIT_SUB:
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
	setBit(PSW, 13, less);
	
	bool equal = op1 == op2;
	setBit(PSW, 12, equal);

	bool greater = op1 > op2;
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
	if (address >= emulator.memorySize) {
		printf(TERM_BOLD_RED "Instruction address 0x%X out of bounds (0x%X)\n" TERM_RESET, address, emulator.memorySize);
		return;
	}

	// Obtém a instrução no endereço
	uint32_t instruction = emulator.memory[address];
					
	// Extrai da instrução os bits do código de operação e argumento X
	uint8_t opcode = (instruction & 0xF000) >> 12;
	uint16_t argument = (instruction & 0x0FFF);

	// Buffer de strings para imprimir a mensagem de uma vez só
	StringBuffer msgBuffer;
	stbInit(&msgBuffer);

	// Obtém o breakpoint configurado nesse endereço se houver
	Breakpoint* bp = emuGetBreakpoint(address);

	if (bp) {
		if (bp->hits == 0) {
			// Se houver um breakpoint desativado, imprime o endereço em roxo
			stbAppend(&msgBuffer, "§D{%3Xh}§5 ", address);
		} else {
			// Se ele for ativo, imprime em vermelho
			stbAppend(&msgBuffer, "§9{%3Xh}§1 ", address);
		}
	} else {
		// Caso contrário, imprime em branco
		stbAppend(&msgBuffer, "§F[%3Xh]§R ", address);
	}

	stbAppend(&msgBuffer, "%X.%03X: ", opcode, argument);
	
	// Transforma a instrução em string e adiciona na string da mensagem
	StringBuffer disassembly = emuDisassembly(instruction);
	stbAppendBuffer(&msgBuffer, &disassembly);
	stbFree(&disassembly);

	stbColorize(&msgBuffer, ENABLE_COLORS);
	printf("%s\n" TERM_RESET, msgBuffer.array);
	stbFree(&msgBuffer);
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
		
	// Obtém o nome da instrução pelo opcode
	const char* name = INSTRUCTION_NAMES[opcode];

	// As instruções por padrão sairão em azul claro
	stbAppend(buffer, TERM_CYAN);

	switch(opcode) {
	case OPCODE_NOP:
		stbAppend(buffer, TERM_BOLD_BLACK "%s ", name);
		break;

	case OPCODE_LDA:
		stbAppend(buffer, "%s [%Xh]", name, argument);
		break;

	case OPCODE_STA:
		stbAppend(buffer, "%s [%Xh]", name, argument);
		break;

	case OPCODE_JMP:
		stbAppend(buffer, "%s %Xh", name, argument);
		break;

	case OPCODE_JNZ:
		stbAppend(buffer, "%s %Xh", name, argument);
		break;

	case OPCODE_RET:
		stbAppend(buffer, "%s", name);
		break;

	case OPCODE_ARIT:
		stbAppend(buffer, "%s ", name);

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
			stbAppend(buffer, ARIT_EXT_FMT[bitsOpr],
				REGISTER_NAMES[bitsDst],
				REGISTER_NAMES[bitsOp1],
				(op2zero) ? "0" : REGISTER_NAMES[bitsOp2 & 0b011]
			);
		} else {
			// Imprime em sequência OPERAÇÃO, RES, OP1, OP2
			stbAppend(buffer, "%s, ", ARIT_OP_NAMES[bitsOpr]);
			stbAppend(buffer, "%s, ", REGISTER_NAMES[bitsDst]);
			stbAppend(buffer, "%s, ", REGISTER_NAMES[bitsOp1]);
			stbAppend(buffer, "%s ", (op2zero) ? "zero" : REGISTER_NAMES[bitsOp2 & 0b011]);
		}
		break;

	case OPCODE_HLT:
		stbAppend(buffer, "%s", name);
		break;

	// Instrução desconhecida
	default:
		stbAppend(buffer, TERM_BOLD_YELLOW "%s :: %X.%03X", name, opcode, argument);
		break;
	}

	return bufferStg;
}

/// @brief Configura um ponto de parada na memória do emulador.
/// @param addr Posição na memória onde a execução vai parar.
/// @param hits Número máximo de vezes que esse breakpoint será acertado antes de ser desativado.
/// Se o número de hits for -1, o breakpoint é considerado infinito e nunca será desativado.
/// Se hits for 0, o breakpoint será configurado já desativado.
void emuSetBreakpoint(uint16_t addr, int hits) {
	// Se um breakpoint com esse endereço já existe, só mude o número de hits dele
	for (int i = 0; i < emulator.breakpoints.size; i++) {
		Breakpoint* bp = (Breakpoint*) emulator.breakpoints.array[i];
		if (bp->address == addr) {
			bp->hits = hits;
			return;
		}
	}

	// Cria um novo breakpoint
	Breakpoint* bp = (Breakpoint*) malloc(sizeof(Breakpoint));
	bp->address = addr;
	bp->hits = hits;
	vecAdd(&emulator.breakpoints, bp);
}

/// @brief Remove um breakpoint previamente configurado. Se o breakpoint não existe, não faz nada.
/// @param addr O endereço do breakpoint a remover
/// @return Um booleano se o breakpoint existia ou não 
bool emuRemoveBreakpoint(uint16_t addr) {
	// Faz uma busca linear no vetor de brakpoints e remove o de endereço igual
	for (int i = 0; i < emulator.breakpoints.size; i++) {
		Breakpoint* bp = (Breakpoint*)emulator.breakpoints.array;

		if (bp->address == addr) {
			free(bp);
			vecRemove(&emulator.breakpoints, i);
			return true;
		}
	}

	return false;
}

/// @brief Obtém o breakpoint setado no endereço de memória especificado.
/// @param addr O endereço de memória do emulador do breakpoint a obter.
/// @return Um ponteiro para estrutura do breakpoint se havia um, NULL se não há breakpoint ali.
Breakpoint* emuGetBreakpoint(uint16_t addr) {
	// Faz uma busca linear no vetor de brakpoints e retorna se for encontrado
	for (int i = 0; i < emulator.breakpoints.size; i++) {
		Breakpoint* bp = (Breakpoint*)emulator.breakpoints.array[i];
		if (bp->address == addr) {
			return bp;
		}
	}

	return NULL;
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

	// Imprime a mensagem de CPU FAULT no terminal
	StringBuffer sb;
	stbInit(&sb);
	stbAppend(&sb, TERM_BOLD_RED TERM_BOLD_RED "[ERR!] CPU FAULT: " TERM_RESET);
	stbAppendv(&sb, fmt, args);
	stbAppend(&sb, "\n");
	printf("%s\n", sb.array);
	stbFree(&sb);

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

	StringBuffer sb;
	stbInit(&sb);

	stbAppend(&sb, TERM_BOLD_YELLOW "[WRN!] " TERM_RESET);
	stbAppendv(&sb, fmt, args);
	
	printf("%s\n\n", sb.array);
	stbFree(&sb);

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

// -- Funções de manipulação de StringBuffer --

/// Inicializa um buffer de strings.
void stbInit(StringBuffer* sb) {
	sb->capacity = 2;
	sb->size = 0;
	sb->array = (char*) calloc(sb->capacity, sizeof(char));
}

/// @brief Expande um buffer de strings
/// @param sb O buffer em si
/// @param minRequiredSize O buffer deve crescer no mínimo o suficiente para conter esse valor
void stbGrow(StringBuffer* sb, size_t minRequiredSize) {
	// Heurística para determinar o fator de crescimento
	size_t growthAmount = minRequiredSize * 2;
	if (growthAmount < 8) growthAmount = 8;

	sb->capacity += growthAmount;
	sb->array = (char*) realloc(sb->array, sb->capacity * sizeof(char));
}

/// @brief Concatena no buffer uma string formatada
/// @param sb O buffer previamente alocado pelo usuário
/// @param fmt A string formatada em estilo printf
/// @return Verdadeiro se a concatenação foi bem sucedida, falso caso contrário.
bool stbAppend(StringBuffer* sb, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);

	bool result = stbAppendv(sb, fmt, args);

	va_end(args);
	return result;
}

/// @brief Concatena a um string buffer uma string formatada com lista de parâmetros.
/// @param sb O buffer em questão previamente inicializado.
/// @param fmt A string de formatos no estilo printf.
/// @param args A lista de argumentos.
/// @return Verdadeiro se a concatenação foi bem sucedida, falso caso contrário.
bool stbAppendv(StringBuffer* sb, const char* fmt, va_list args) {
	// Obtém um ponteiro para a posição atual do buffer e a capacidade restante
	char* ptr = &sb->array[sb->size];
	size_t remaining = sb->capacity - sb->size;

	// Tenta escrever no buffer sem aumentar o tamanho
	va_list argsCopy;
	va_copy(argsCopy, args);
	int strSize = vsnprintf(ptr, remaining, fmt, argsCopy);
	va_end(argsCopy);

	// Erro de formatação. Não foi escrito nada
	if (strSize < 0) return false;

	// Escrita bem sucedida
	if (strSize < remaining) {
		sb->size += strSize;
		return true;
	}

	// Expandimos o buffer e tentamos escrever no buffer mais uma vez
	stbGrow(sb, strSize);
	ptr = sb->array + sb->size;
	remaining = sb->capacity - sb->size;
	va_list argsCopy2;
	va_copy(argsCopy2, args);
	strSize = vsnprintf(ptr, remaining, fmt, args);
	va_end(argsCopy2);

	// Erro de formatação ou algum outro erro
	if (strSize < 0) return false;

	assert(strSize < remaining);
	sb->size += strSize;
	return true;
}

/// @brief Anexa os conteúdos de outro buffer a este buffer
/// @param sb O buffer a ser expandido com conteúdos
/// @param buffer O buffer a ser adicionado ao primeiro
void stbAppendBuffer(StringBuffer* sb, StringBuffer* buffer) {
	size_t newSize = sb->size + buffer->size;
	if (sb->capacity < newSize) {
		stbGrow(sb, newSize);
	}

	char* dst = sb->array + sb->size;
	strcpy(dst, buffer->array);
	sb->size += buffer->size;
}

/// @brief Substitui os símbols §X de cores pelos códigos ANSI necessários para gerar as cores.
/// @param buffer o Buffer a ser colorizado.
/// @param outputColors Se as cores devem ser só descartadas ao invés de traduzidas para cores
/// no terminal.
void stbColorize(StringBuffer* buffer, bool outputColors) {
	// Rouba o array de caracteres e reinicializa o buffer do usuário
	char* array = buffer->array;
	buffer->array = NULL;
	stbFree(buffer);
	stbInit(buffer);

	// Adiciona a primeira parte da string, anterior possívelmente ao símbolo §
	char* token = strtok(array, "§");
	if (array[0] == token[0]) {
		stbAppend(buffer, token);
		token = strtok(NULL, "§");
	}

	// Itera todas as partes da string precedidas de §
	while (token) {
		int color = token[0];
		if (outputColors) {
			if (color == 'R') {
				stbAppend(buffer, "\033[0m");
			} else if (color >= '8') {
				if (color >= 'A') color -= 'A' - 2;
				else color -= '8';
				stbAppend(buffer, "\033[1;%im", 30 + color);
			} else {
				color -= '0';
				stbAppend(buffer, "\033[0;%im", 30 + color);
			}
		}

		stbAppend(buffer, token + 1);

		token = strtok(NULL, "§");
	}
}

/// Libera a memória utilizada pelo buffer de strings
void stbFree(StringBuffer* sb) {
	free(sb->array);
	sb->array = NULL;
	sb->size = 0;
	sb->capacity = 0;
}


/// @brief Seta um bit do número passado
/// @param reg Um pointeiro para um número o qual se deseja setar o bit
/// @param bit A posição do bit. O bit 0 é o bit menos significante e o 15 o mais significante
/// @param value Um booleano 1 ou 0 com o valor desejado
void setBit(uint16_t* reg, int bit, bool value) {
	uint16_t num = *reg;
	num &= ~(1UL << bit);            // Clear bit first
	num |= ((uint32_t)value) << bit; // Set bit
	*reg = num;
}

/// @brief Obtém o valor do bit em uma posição no valor
/// @param value O valor o qual se deseja extrair o bit
/// @param bit A posição do bit desejado
/// @return Um bool true se o bit está setado, false, caso contrário
bool getBit(uint16_t value, int bit) {
	return (value >> bit) & 1UL;
}

/// @brief Converte toda uma string para minúsculo
void toLowerCase(char* str) {
	while (*str) {
		*str = tolower(*str);
		str++;
	}
}

/// @brief Verifica se duas strings são iguais
bool strEquals(const char* a, const char* b) {
	return strcmp(a, b) == 0;
}

/// @brief Inicializa um vetor dinâmico
/// @param vec O próprio vetor a ser inicializado
void vecInit(Vector* vec) {
	vec->array = (void**) malloc(sizeof(void*));
	vec->capacity = 1;
	vec->size = 0;
}

/// @brief Libera a memória ocupada por um vetor dinâmico e libera todos os ponteiros contidos.
void vecFree(Vector* vec) {
	for (int i = 0; i < vec->size; i++) {
		free(vec->array[i]);
	}
	free(vec->array);
	
	vec->array = NULL;
	vec->capacity = -1;
	vec->size = -1;
}

/// @brief Cresce a capacidade do vetor
void vecGrow(Vector* vec) {
	vec->capacity *= 2;
	vec->array = realloc(vec->array, sizeof(void*) * vec->capacity);
}

/// @brief Adiciona um elemento no vetor
void vecAdd(Vector* vec, void* elem) {
	if (vec->capacity == vec->size) {
		vecGrow(vec);
	}

	vec->array[vec->size] = elem;
	vec->size++;
}

/// @brief Remove um um elemento pelo seu índice
/// @param index O índice do elemento a ser removido 
void vecRemove(Vector* vec, int index) {
	// Assegura a validade do índice
	assert(index < vec->size);

	// Shift all elements to the left by one position
	for (int i = index; i < vec->size - 1; i++) {
		vec->array[i] = vec->array[i + 1];
	}

	vec->size--;
}

/// @brief Imprime uma string formatada no console utilizando § para as cores estilizadas.
void prints(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);

	StringBuffer buffer;
	stbInit(&buffer);

	stbAppendv(&buffer, fmt, args);
	stbColorize(&buffer, terminalColorsEnabled);
	printf("%s", buffer.array);

	stbFree(&buffer);

	va_end(args);
}
