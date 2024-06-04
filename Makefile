all: release

CFLAGS=-std=c99 -Wall
# Torna esses warnings em erros
CFLAGS+=-Werror=return-type -Werror=incompatible-pointer-types
# Desativa esse warning
CFLAGS+=-Wno-unused-variable

release: ep.exe

debug: epd.exe
debug: CFLAGS+=-g

test: ep.exe
	ep sample.mem

test1: ep.exe
	ep tests/overflowEesr-testado.mem

test2: ep.exe
	ep tests/overflowEesr.mem

test3: ep.exe
	ep tests/movRegAcc.mem

test4: ep.exe
	ep tests/initEPSW.mem

random: ep.exe
	ep tests/random.mem

ep.exe: EP1.c build/StringBuffer.o
	gcc EP1.c driverEP1.c build/StringBuffer.o -o ep.exe $(CFLAGS)

epd.exe: EP1.c build/StringBuffer.o
	gcc EP1.c driverEP1.c build/StringBuffer.o -o epd.exe $(CFLAGS) 

build/StringBuffer.o: StringBuffer.c StringBuffer.h
	gcc -c StringBuffer.c -o build/StringBuffer.o $(CFLAGS)

clean:
	del *.exe
	del build\*.exe
