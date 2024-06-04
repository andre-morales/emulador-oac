#include "StringBuffer.h"
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

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