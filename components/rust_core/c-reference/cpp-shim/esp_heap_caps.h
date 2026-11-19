#pragma once
#include <stdlib.h>
#define MALLOC_CAP_8BIT (1 << 2)
#define MALLOC_CAP_DMA (1 << 3)
#define MALLOC_CAP_SPIRAM (1 << 10)
static inline void *heap_caps_malloc(size_t s, unsigned caps) { (void) caps; return malloc(s); }
static inline void *heap_caps_calloc(size_t n, size_t s, unsigned caps) { (void) caps; return calloc(n, s); }
static inline void *heap_caps_realloc(void *p, size_t s, unsigned caps) { (void) caps; return realloc(p, s); }
static inline void heap_caps_free(void *p) { free(p); }
