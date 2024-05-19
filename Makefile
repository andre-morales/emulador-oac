all: ep.exe

CFLAGS =-std=c99 -Wall
# Torna esses warnings em erros
CFLAGS +=-Werror=return-type -Werror=incompatible-pointer-types
# Desativa esse warning
CFLAGS +=-Wno-unused-variable

test1: ep.exe
	ep img/overflowEesr-testado.mem

test2: ep.exe
	ep img/overflowEesr.mem

test3: ep.exe
	ep img/movRegAcc.mem

test4: ep.exe
	ep img/initEPSW.mem

random: ep.exe
	ep img/random.mem

ep.exe: EP1.c
	gcc EP1.c driverEP1.c $(CFLAGS) -o ep.exe

clean:
	del *.exe
