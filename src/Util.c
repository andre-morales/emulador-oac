#include "Util.h"
#include "StringBuffer.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

extern bool terminalColorsEnabled;

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
