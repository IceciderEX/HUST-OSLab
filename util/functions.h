#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_

// ROUNDUP(stval, PGSIZE) 会将地址stval对齐到下一个页面的起始地址
#define ROUNDUP(a, b) ((((a)-1) / (b) + 1) * (b))
// ROUNDDOWN(stval, PGSIZE) is the page begining address(所在页面的起始地址)
#define ROUNDDOWN(a, b) ((a) / (b) * (b))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

char* safestrcpy(char*, const char*, int);

#endif