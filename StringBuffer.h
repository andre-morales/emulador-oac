#pragma once
#include <stdint.h>
#include <stdbool.h>

/// @brief Permite a manipulação e concatenação de strings formatadas
typedef struct StringBufferT {
	char* array;
	size_t size;
	size_t capacity;
} StringBuffer;

// -- Funções auxiliares de manipulação de strings

void stbInit(StringBuffer*);
void stbGrow(StringBuffer* sb, size_t minRequiredSize);
bool stbAppend(StringBuffer*, const char* fmt, ...);
bool stbAppendv(StringBuffer* sb, const char* fmt, va_list args);
void stbAppendBuffer(StringBuffer* sb, StringBuffer* buffer);
void stbColorize(StringBuffer* sb, bool outputColors);
void stbFree(StringBuffer*);