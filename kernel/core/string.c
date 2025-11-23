/*
 * AArch64 Bare Metal OS - String Utility Functions
 */

#include "kernel.h"

/*
 * Set memory to a value
 */
void *memset(void *s, int c, size_t n)
{
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

/*
 * Copy memory
 */
void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

/*
 * Get string length
 */
size_t strlen(const char *s)
{
    size_t len = 0;
    while (*s++) {
        len++;
    }
    return len;
}

/*
 * Compare strings
 */
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}
