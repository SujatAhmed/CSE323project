#include "string.h"

void *memset(void *dst, int c, size_t n) {
    uint8_t *p = dst;
    while (n--) *p++ = (uint8_t)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *p = a, *q = b;
    while (n--) {
        if (*p != *q) return *p - *q;
        p++; q++;
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (*s++) n++;
    return n;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dst;
}

char *strcat(char *dst, const char *src) {
    char *d = dst + strlen(dst);
    while ((*d++ = *src++));
    return dst;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *itoa(int64_t value, char *buf, int base) {
    char tmp[65];
    int i = 0;
    int neg = (base == 10 && value < 0);
    uint64_t uval = neg ? -(uint64_t)value : (uint64_t)value;

    if (uval == 0) { tmp[i++] = '0'; }
    while (uval) {
        int rem = uval % base;
        tmp[i++] = rem < 10 ? '0' + rem : 'a' + rem - 10;
        uval /= base;
    }
    if (neg) tmp[i++] = '-';

    char *p = buf;
    while (i--) *p++ = tmp[i];
    *p = '\0';
    return buf;
}

char *utoa(uint64_t value, char *buf, int base) {
    char tmp[65];
    int i = 0;
    if (value == 0) { tmp[i++] = '0'; }
    while (value) {
        int rem = value % base;
        tmp[i++] = rem < 10 ? '0' + rem : 'a' + rem - 10;
        value /= base;
    }
    char *p = buf;
    while (i--) *p++ = tmp[i];
    *p = '\0';
    return buf;
}

int atoi(const char *s) {
    int n = 0, neg = 0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return neg ? -n : n;
}
