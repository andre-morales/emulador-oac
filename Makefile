.PHONY: all release debug test1 test2 test3 test4 random clean

CFLAGS=-std=c99 -Wall
# Torna esses warnings em erros
CFLAGS+=-Werror=return-type -Werror=incompatible-pointer-types
# Desativa warnings para variáveis não utilizadas
CFLAGS+=-Wno-unused-variable

SOURCES=src/Emulador.c src/driverEP1.c

ifeq ($(OS),Windows_NT)
	TARGET=emul.exe
	DTARGET=emuld.exe
	NULLDEV=nul
else 
	TARGET=emul
	DTARGET=emuld
	NULLDEV=/dev/null
endif

all: release
release: $(TARGET)
debug: $(DTARGET)

test: release
	emul sample.mem

test1: release
	emul tests/overflowEesr-testado.mem

test2: release
	emul tests/overflowEesr.mem

test3: release
	emul tests/movRegAcc.mem

test4: release
	emul tests/initEPSW.mem

random: release
	emul tests/random.mem

$(TARGET): $(SOURCES)
	echo $(TARGET)
	gcc $(SOURCES) -o emul $(CFLAGS)

$(DTARGET): CFLAGS+=-g
$(DTARGET): $(SOURCES)
	gcc $(SOURCES) -o emuld $(CFLAGS) 

clean:
	@echo Cleaning all build files...
	-@rm *.exe 2> $(NULLDEV)
	-@del *.exe 2> $(NULLDEV)
	-@rm emul 2> $(NULLDEV)
	-@del emul 2> $(NULLDEV)
	-@rm emuld 2> $(NULLDEV)
	-@del emuld 2> $(NULLDEV)
	@echo Done!
