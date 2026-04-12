#ifndef ARENA_H_
#define ARENA_H_

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#ifndef ARDEF
#define ARDEF
#endif // ARDEF

typedef struct Arena Arena;
struct Arena {
    size_t capacity;
    size_t count;
    uint8_t *items;
    Arena *next;
};

ARDEF Arena* arena_new(size_t capacity);
ARDEF void* arena_alloc(Arena *arena, size_t size);
ARDEF char* arena_strdup(Arena *arena, const char *s);
ARDEF void arena_free(Arena *arena);

#ifdef ARENA_IMPLEMENTATION

#ifndef ARENA_INIT_CAP
#define ARENA_INIT_CAP (1024*8)
#endif // ARENA_INIT_CAP

#ifndef ARENA_GROW_SIZE
#define ARENA_GROW_SIZE 2
#endif // ARENA_GROW_SIZE

#ifndef ARENA_ALIGN
#define ARENA_ALIGN (sizeof(max_align_t))
static_assert((ARENA_ALIGN & (ARENA_ALIGN - 1)) == 0, "Align is not a power of two");
#endif // ARENA_ALIGN

ARDEF Arena* arena_new(size_t capacity)
{
    if(capacity < ARENA_INIT_CAP) capacity = ARENA_INIT_CAP;
    Arena *arena = malloc(sizeof(Arena));
    if(!arena) return NULL;
    arena->capacity = capacity;
    arena->count = 0;
    arena->items = malloc(capacity);
    if(!arena->items) return NULL;
    arena->next = NULL;
    return arena;
}

ARDEF uintptr_t arena__align_up(const uintptr_t ptr)
{
    const size_t mask = ARENA_ALIGN - 1;
    return (ptr + mask) & ~mask;
}

ARDEF void* arena_alloc(Arena *arena, size_t size)
{
    const uintptr_t aligned = arena__align_up((uintptr_t) (arena->items + arena->count));
    const size_t aligned_offset = (aligned - (uintptr_t) arena->items) + size;
    if(aligned_offset <= arena->capacity)
    {
        arena->count = aligned_offset;
        return (void*) aligned;
    }

    if(arena->next) return arena_alloc(arena->next, size);

    size_t new_size = arena->capacity * ARENA_GROW_SIZE;
    while(new_size <= size) new_size *= ARENA_GROW_SIZE;
    arena->next = arena_new(new_size);
    if(!arena->next) return NULL;
    
    return arena_alloc(arena->next, size);
}

ARDEF char* arena_strdup(Arena *arena, const char *s)
{
    const size_t len = strlen(s);
    char *buf = arena_alloc(arena, len + 1);
    memcpy(buf, s, len + 1);

    return buf;
}

ARDEF void arena_free(Arena *arena)
{
    if(!arena) return;

    arena_free(arena->next);
    free(arena->items);
    free(arena);
}

#endif // ARENA_IMPLEMENTATION

#endif // ARENA_H_
