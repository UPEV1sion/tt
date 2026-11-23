#ifndef FLAG_H_
#define FLAG_H_

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FLAGDEF
#define FLAGDEF
#endif // FLAGDEF

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
    bool seen;
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

FLAGDEF void flag_decl_bool(Flags *flags,
                            bool *val,
                            const char *short_flag,
                            const char *long_flag,
                            bool default_val,
                            const char *description);
FLAGDEF void flag_decl_str(Flags *flags,
                           char **val,
                           const char *short_flag,
                           const char *long_flag,
                           char *default_val,
                           const char *description);
FLAGDEF void flag_decl_double(Flags *flags,
                              double *val,
                              const char *short_flag,
                              const char *long_flag,
                              double default_val,
                              const char *description);
FLAGDEF void flag_decl_int(Flags *flags,
                           int64_t *val,
                           const char *short_flag,
                           const char *long_flag,
                           int64_t default_val,
                           const char *description);
FLAGDEF void flag_decl_uint(Flags *flags,
                            uint64_t *val,
                            const char *short_flag,
                            const char *long_flag,
                            uint64_t default_val,
                            const char *description);

FLAGDEF int flag_parse(Flags *flags, int argc, char **argv);
FLAGDEF void flag_free(Flags *flags);

FLAGDEF void flag_print_info(Flags *flags, const char *header);

FLAGDEF bool flag_seen(Flags *flags, const char *short_flag, const char *long_flag);

#ifdef FLAG_IMPLEMENTATION

#define FLAG__DA_INIT_CAP 32
#define FLAG__DA_GROW_SIZE 2

#define flag__da_reserve(da, expected)                                                 \
    do                                                                                 \
    {                                                                                  \
        if ((expected) > (da)->capacity)                                               \
        {                                                                              \
            if ((da)->capacity == 0) (da)->capacity = FLAG__DA_INIT_CAP;               \
            while ((expected) > (da)->capacity) (da)->capacity *= FLAG__DA_GROW_SIZE;  \
            (da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items)); \
            assert((da)->items && "ERROR: out of mem!\n");                             \
        }                                                                              \
    } while (0)

#define flag__da_append(da, item)                \
    do                                           \
    {                                            \
        flag__da_reserve((da), (da)->count + 1); \
        (da)->items[(da)->count++] = (item);     \
    } while (0)

#define flag__da_foreach(type, name, da) for (type *name = (da)->items; name < (da)->items + (da)->count; ++name)

#define flag__da_free(da) free((da)->items)

#define FLAG__ALPHABET_SIZE 256
#define flag__shift_args(argc, argv) ((argc)--, *(argv)++)

FLAGDEF Flag flag__new_flag_bool(bool *val,
                                 const char *short_flag,
                                 const char *long_flag,
                                 const bool default_val,
                                 const char *description)
{
    return (Flag){
        .type        = FLAG_BOOL,
        .user_ptr    = val,
        .short_flag  = short_flag,
        .long_flag   = long_flag,
        .defaults.b  = default_val,
        .description = description,
    };
}

FLAGDEF Flag flag__new_flag_str(char **val,
                                const char *short_flag,
                                const char *long_flag,
                                char *default_val,
                                const char *description)
{
    return (Flag){
        .type        = FLAG_STRING,
        .user_ptr    = val,
        .short_flag  = short_flag,
        .long_flag   = long_flag,
        .defaults.s  = default_val,
        .description = description,
    };
}

FLAGDEF Flag flag__new_flag_int(int64_t *val,
                                const char *short_flag,
                                const char *long_flag,
                                const int64_t default_val,
                                const char *description)
{
    return (Flag){
        .type        = FLAG_INT,
        .user_ptr    = val,
        .short_flag  = short_flag,
        .long_flag   = long_flag,
        .defaults.i  = default_val,
        .description = description,
    };
}

FLAGDEF Flag flag__new_flag_uint(uint64_t *val,
                                 const char *short_flag,
                                 const char *long_flag,
                                 const uint64_t default_val,
                                 const char *description)
{
    return (Flag){
        .type        = FLAG_UINT,
        .user_ptr    = val,
        .short_flag  = short_flag,
        .long_flag   = long_flag,
        .defaults.u  = default_val,
        .description = description,
    };
}

FLAGDEF Flag flag__new_flag_double(double *val,
                                   const char *short_flag,
                                   const char *long_flag,
                                   const double default_val,
                                   const char *description)
{
    return (Flag){
        .type        = FLAG_DOUBLE,
        .user_ptr    = val,
        .short_flag  = short_flag,
        .long_flag   = long_flag,
        .defaults.d  = default_val,
        .description = description,
    };
}

FLAGDEF void flag_decl_bool(Flags *flags,
                            bool *val,
                            const char *short_flag,
                            const char *long_flag,
                            bool default_val,
                            const char *description)
{
    if (!short_flag && !long_flag) fprintf(stderr, "WARNING: no short nor long flag provided!\n");

    const Flag f = flag__new_flag_bool(val, short_flag, long_flag, default_val, description);
    flag__da_append(flags, f);
}

FLAGDEF void flag_decl_str(Flags *flags,
                           char **val,
                           const char *short_flag,
                           const char *long_flag,
                           char *default_val,
                           const char *description)
{
    if (!short_flag && !long_flag) fprintf(stderr, "WARNING: no short nor long flag provided!\n");

    const Flag f = flag__new_flag_str(val, short_flag, long_flag, default_val, description);
    flag__da_append(flags, f);
}

FLAGDEF void flag_decl_double(Flags *flags,
                              double *val,
                              const char *short_flag,
                              const char *long_flag,
                              double default_val,
                              const char *description)
{
    if (!short_flag && !long_flag) fprintf(stderr, "WARNING: no short nor long flag provided!\n");

    const Flag f = flag__new_flag_double(val, short_flag, long_flag, default_val, description);
    flag__da_append(flags, f);
}

void flag_decl_int(Flags *flags,
                   int64_t *val,
                   const char *short_flag,
                   const char *long_flag,
                   int64_t default_val,
                   const char *description)
{
    if (!short_flag && !long_flag) fprintf(stderr, "WARNING: no short nor long flag provided!\n");

    const Flag f = flag__new_flag_int(val, short_flag, long_flag, default_val, description);
    flag__da_append(flags, f);
}

FLAGDEF void flag_decl_uint(Flags *flags,
                            uint64_t *val,
                            const char *short_flag,
                            const char *long_flag,
                            uint64_t default_val,
                            const char *description)
{
    if (!short_flag && !long_flag) fprintf(stderr, "WARNING: no short nor long flag provided!\n");

    const Flag f = flag__new_flag_uint(val, short_flag, long_flag, default_val, description);
    flag__da_append(flags, f);
}

FLAGDEF bool parse_arg(void *val, FlagType type, char *arg)
{
    if ('-' == *arg && !(FLAG_INT == type || FLAG_DOUBLE == type)) return false;

    switch (type)
    {
    case FLAG_BOOL: {
        bool b = 0 == strcmp(arg, "true");
        if (!(b || 0 == strcmp(arg, "false"))) return false;
        *(bool *)val = b;
        return true;
    }
    break;
    case FLAG_STRING: {
        *(char **)val = arg;
        return true;
    }
    break;
    case FLAG_DOUBLE: {
        char *endptr;
        double d = strtod(arg, &endptr);
        if (0 != *endptr) return false;
        *(double *)val = d;
        return true;
    }
    break;
    case FLAG_INT: {
        char *endptr;
        long long d = strtoll(arg, &endptr, 10);
        if (0 != *endptr) return false;
        *(int64_t *)val = d;
        return true;
    }
    break;
    case FLAG_UINT: {
        char *endptr;
        unsigned long long d = strtoull(arg, &endptr, 10);
        if (0 != *endptr) return false;
        *(uint64_t *)val = d;
        return true;
    }
    break;
    }

    return false;
}

FLAGDEF Flag *find_long_flag(Flags *flags, const char *name, size_t name_len)
{
    flag__da_foreach(Flag, flag, flags)
    {
        if (!flag->long_flag) continue;
        if (strlen(flag->long_flag) == name_len && strncmp(flag->long_flag, name, name_len) == 0)
        {
            return flag;
        }
    }
    return NULL;
}

FLAGDEF int apply_flag_val(Flag *f, const char *flag_label, int *argc, char ***argv, const char *attached_val)
{
    char *val_str = NULL;

    if (attached_val)
    {
        val_str = (char *)attached_val;
    }
    else if (FLAG_BOOL != f->type)
    {
        if (*argc <= 0)
        {
            fprintf(stderr, "ERROR: flag '%s' requires an argument\n", flag_label);
            return 1;
        }
        val_str = flag__shift_args(*argc, *argv);
    }
    else
    {
        if (*argc > 0 && (0 == strcmp(**argv, "true") || 0 == strcmp(**argv, "false")))
        {
            val_str = flag__shift_args(*argc, *argv);
        }
        else
        {
            *(bool *)f->user_ptr = true;
            f->seen              = true;
            return 0;
        }
    }

    if (!parse_arg(f->user_ptr, f->type, val_str))
    {
        fprintf(stderr, "ERROR: invalid value '%s' for flag '%s'\n", val_str, flag_label);
        return 1;
    }

    f->seen = true;
    return 0;
}

FLAGDEF int flag_parse(Flags *flags, int argc, char **argv)
{
    Flag *short_flags[FLAG__ALPHABET_SIZE] = {0};

    flag__da_foreach(Flag, flag, flags)
    {
        flag->seen = false;
        if (flag->short_flag)
        {
            assert(strlen(flag->short_flag) == 2 && '-' == flag->short_flag[0] && 0 == flag->short_flag[2]);
            short_flags[(unsigned char)flag->short_flag[1]] = flag;
        }

        if (flag->long_flag)
        {
            assert(strlen(flag->long_flag) >= 2 && '-' == flag->long_flag[0] && '-' == flag->long_flag[1]);
        }

        switch (flag->type)
        {
        case FLAG_BOOL:   *(bool *)flag->user_ptr = flag->defaults.b; break;
        case FLAG_STRING: *(char **)flag->user_ptr = flag->defaults.s; break;
        case FLAG_DOUBLE: *(double *)flag->user_ptr = flag->defaults.d; break;
        case FLAG_INT:    *(int64_t *)flag->user_ptr = flag->defaults.i; break;
        case FLAG_UINT:   *(uint64_t *)flag->user_ptr = flag->defaults.u; break;
        }
    }

    while (argc > 0)
    {
        char *flag            = flag__shift_args(argc, argv);
        const size_t flag_len = strlen(flag);
        if (flag_len < 2)
        {
            fprintf(stderr, "ERROR: malformed flag '%s'\n", flag);
            return 1;
        }

        if ('-' == flag[1])
        {
            if (2 == flag_len)
            {
                fprintf(stderr, "ERROR: standalone '--' is not supported\n");
                return 1;
            }

            const char *eq        = strchr(flag, '=');
            const size_t name_len = eq ? (size_t)(eq - flag) : strlen(flag);
            if (0 == name_len)
            {
                fprintf(stderr, "ERROR: malformed long flag '%s'\n", flag);
                return 1;
            }

            Flag *f = find_long_flag(flags, flag, name_len);
            if (!f)
            {
                fprintf(stderr, "ERROR: unknown flag '%s'\n", flag);
                return 1;
            }

            const char *attached_val = eq ? (eq + 1) : NULL;
            if (0 != apply_flag_val(f, flag, &argc, &argv, attached_val))
            {
                return 1;
            }
            continue;
        }

        if (2 == flag_len || '=' == flag[2])
        {
            Flag *f = short_flags[(unsigned char)flag[1]];
            if (!f)
            {
                fprintf(stderr, "ERROR: unknown flag '%s'\n", flag);
                return 1;
            }

            char *val_str = NULL;
            if ('=' == flag[2])
            {
                val_str = flag + 3;
            }
            else if (FLAG_BOOL != f->type)
            {
                if (argc <= 0)
                {
                    fprintf(stderr, "ERROR: flag '%s' requires an argument\n", flag);
                    return 1;
                }
                val_str = flag__shift_args(argc, argv);
            }
            else
            {
                if (argc > 0 && (0 == strcmp(*argv, "true") || 0 == strcmp(*argv, "false")))
                {
                    val_str = flag__shift_args(argc, argv);
                }
                else
                {
                    *(bool *)f->user_ptr = true;
                    f->seen              = true;
                    continue;
                }
            }

            if (!parse_arg(f->user_ptr, f->type, val_str))
            {
                fprintf(stderr, "ERROR: invalid value '%s' for flag '%s'\n", val_str, flag);
                return 1;
            }
            f->seen = true;
            continue;
        }

        for (size_t i = 1; i < flag_len; ++i)
        {
            const char c = flag[i];
            Flag *f      = short_flags[(unsigned char)c];
            if (!f)
            {
                fprintf(stderr, "ERROR: invalid flag '-%c'\n", c);
                return 1;
            }
            if (FLAG_BOOL != f->type)
            {
                fprintf(stderr, "ERROR: flag '-%c' cannot be bundled or take '=' in '%s'\n", c, flag);
                return 1;
            }
            *(bool *)f->user_ptr = true;
            f->seen              = true;
        }
    }

    return 0;
}

FLAGDEF void flag_free(Flags *flags)
{
    flag__da_free(flags);
}

FLAGDEF void flag_print_info(Flags *flags, const char *header)
{
    size_t max_len = 0;
    flag__da_foreach(Flag, flag, flags)
    {
        if (flag->long_flag)
        {
            const size_t len = strlen(flag->long_flag);
            if (len > max_len) max_len = len;
        }
    }

    puts(header);
    flag__da_foreach(Flag, flag, flags)
    {
        if (!flag->short_flag && !flag->long_flag) continue;

        const char *short_f = flag->short_flag ? flag->short_flag : "  ";
        const char *sep     = (flag->short_flag && flag->long_flag) ? ", " : "  ";
        const char *long_f  = flag->long_flag ? flag->long_flag : "";

        printf("    %s%s%-*s    %s\n", short_f, sep, (int)max_len, long_f, flag->description);
    }
}

FLAGDEF bool flag_seen(Flags *flags, const char *short_flag, const char *long_flag)
{
    flag__da_foreach(Flag, flag, flags)
    {
        const bool short_matches = (short_flag && flag->short_flag) && (0 == strcmp(flag->short_flag, short_flag));
        const bool long_matches  = (long_flag && flag->long_flag) && (0 == strcmp(flag->long_flag, long_flag));
        if (short_flag && long_flag && (short_matches ^ long_matches))
        {
            fprintf(stderr,
                    "WARNING: flag inconsistency detected! '%s' and '%s' do not match the declared flag!\n",
                    short_flag,
                    long_flag);
        }
        if (short_matches || long_matches) return flag->seen;
    }

    return false;
}

#endif // FLAG_IMPLEMENTATION

#endif // FLAG_H_
