#pragma once
#include <stdint.h>
#include <stdbool.h>

/// @brief Vetor dinâmico de inteiros
typedef struct VectorT {
	void** array;
	int size;
	int capacity;
} Vector;

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