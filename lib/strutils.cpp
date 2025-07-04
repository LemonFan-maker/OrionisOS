#include "strutils.h"

char *itob(uint64_t num, uint64_t base) {
    static char hold[] = "0123456789ABCDEF";
    static char buffer[50];
    char *str;
    str = &buffer[49];
    *str = '\0';
    do {
        *--str = hold[num % base];
        num /= base;
    } while (num != 0);
    return str;
}
