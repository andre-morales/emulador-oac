all: ep.exe

CFLAGS =-std=c99 -Wall
# Torna esses warnings em erros
CFLAGS +=-Werror=return-type -Werror=incompatible-pointer-types
# Desativa esse warning
CFLAGS +=-Wno-unused-variable

test1: ep.exe sample1.ram
	ep.exe sample1.ram 

test4: ep.exe
	ep img/initEPSW.mem

ep.exe: EP1.c
	gcc EP1.c driverEP1.c $(CFLAGS) -o ep.exe

clean:
	del *.exe
