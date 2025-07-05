#pragma once
#include <stddef.h> // size_t
#include <stdint.h> // integer types

#ifdef __cplusplus

extern "C" {
    #endif
    int toupper(int c);
    int tolower(int c);

    char *strcpy(char *dest, const char *src);
    char *strncpy(char *dest, const char *src, size_t n);

    int strcmp(const char *s1, const char *s2);
    int strncmp(const char *s1, const char *s2, size_t n);

    size_t strlen(const char *str);

    //  声明由 memcpy_asm.asm 实现的 memcpy ---
    void *memcpy(void *dest, const void *src, size_t n);
    void* memmove(void* dest, const void* src, size_t n);
    void *memset(void *s, int c, size_t n);

    #ifdef __cplusplus
}
#endif
