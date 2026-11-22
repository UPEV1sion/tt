#ifndef FLAG_H_
#define FLAG_H_

#include <stdbool.h>
#include <stdint.h>

#include "da.h"

typedef enum {
    FLAG_BOOL,
    FLAG_STRING,
    FLAG_DOUBLE,
    FLAG_INT,
    FLAG_UINT,
} FlagType;

typedef struct {
    FlagType type;
    const char *short_flag;
    const char *long_flag;
    const char *description;
    union {
        char *s;
        bool b;
        double d;
        int64_t i;
        uint64_t u;
    } defaults;
    void *user_ptr;
} Flag;

typedef struct {
    size_t count;
    size_t capacity;
    Flag *items;
} Flags;

void flag_decl_bool(Flags *flags,
                    bool *val,
                    const char *short_flag,
                    const char *long_flag,
                    bool default_val,
                    const char *description);
void flag_decl_str(Flags *flags,
                   char **val,
                   const char *short_flag,
                   const char *long_flag,
                   char *default_val,
                   const char *description);
void flag_decl_double(Flags *flags,
                      double *val,
                      const char *short_flag,
                      const char *long_flag,
                      double default_val,
                      const char *description);
void flag_decl_int(Flags *flags,
                   int64_t *val,
                   const char *short_flag,
                   const char *long_flag,
                   int64_t default_val,
                   const char *description);
void flag_decl_uint(Flags *flags,
                    uint64_t *val,
                    const char *short_flag,
                    const char *long_flag,
                    uint64_t default_val,
                    const char *description);

int flag_parse(Flags *flags, int argc, char **argv);
void flag_free(Flags *flags);

void flag_print_info(Flags *flags, const char *header);

#endif // FLAG_H_
