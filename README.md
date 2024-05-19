# Emulador e Depurador do Processador de OAC-I

Esse projeto implementa um emulador interativo completo do processador 16 bits visto na disciplina ministrada pelo professor Fábio Nakano.
O programa aceita como entrada os conteúdos da memória préviamente programada no simulador e executa sequencialmente as instruções, simulando o processador do Logisim.

## Uso
Este emulador permite controlar a execução do programa em qualquer ponto e visualizar o estado do processador e da memória através dos comandos de depuração.
Todos os comandos podem ser vistos com o emulador parado e digitando ```help``` no prompt.
```
	Pressing CTRL-C at any time will interrupt emulation.
	Pressing it in quick succession will quit the emulator entirely.
	
	help: prints this help guide.
	
	quit, q: quits out of the emulator.
	
	step, s <amount>:
	    Steps through <amount> of instructions and no further.
	
	continue, c:
	    Leaves step-through mode and lets the emulator run freely.
	    Execution will be stopped upon encountering a fault or the user
	    pressing CTRL-C.
	
	reset:
	    Resets the memory state as it were in the beginning of the emulation    
	    and clears all registers.
	
	registers, r: 
	    View the contents of all CPU registers.
	
	memory, m, x <address> [words]:
	    Views the contents of the emulator memory at the given address with an
	       optional amount of words to display.

	disassembly, d [address] [amount]:
	    Disassembles the given amount of instructions at the address specified.
	    If no address is specified, prints the current instruction.

	nobreak: disables emulator pauses on cpu faults.
	
	dobreak: reenables emulator pauses on cpu faults.
```

## Compilação
Para compilar o emulador, deve-se obter os arquivos driverEP1.c e driverEP1.h disponíveis no repositório do professor. O Makefile já vem pronto para compilação e para alguns testes.