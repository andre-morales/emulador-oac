all: release

CFLAGS=-std=c99 -Wall
# Torna esses warnings em erros
CFLAGS+=-Werror=return-type -Werror=incompatible-pointer-types
# Desativa warnings para variáveis não utilizadas
CFLAGS+=-Wno-unused-variable

SOURCES=src/Emulador.c src/driverEP1.c src/StringBuffer.c src/Util.c

release: ep.exe
debug: epd.exe

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

ep.exe: $(SOURCES)
	gcc $(SOURCES) -o ep.exe $(CFLAGS)

epd.exe: CFLAGS+=-g
epd.exe: $(SOURCES)
	gcc $(SOURCES) -o epd.exe $(CFLAGS) 

clean:
	-rm *.exe
	-del *.exe
