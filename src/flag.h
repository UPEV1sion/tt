#ifndef FLAG_H_
#define FLAG_H_

#include <assert.h>
#include <errno.h>
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
    _flag_count,
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
    int argc;
    char **argv;
#ifndef FLAG_NO_PROGRAM_NAME
    const char *program_name;
#endif // FLAG_NO_PROGRAM_NAME
    bool help;
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

FLAGDEF bool flag_seen(Flags *flags, void *flag);
FLAGDEF void flag_rest_args(Flags *flags, int *argc, char ***argv);
#ifndef FLAG_NO_PROGRAM_NAME
FLAGDEF const char *flag_program_name(Flags *flags);
#endif // FLAG_NO_PROGRAM_NAME

#ifdef FLAG_IMPLEMENTATION

#if defined(__cplusplus)
#define FLAG__STATIC_ASSERT(cond, msg) static_assert((cond), msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define FLAG__STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#else
#define FLAG__CAT_(a, b) a##b
#define FLAG__CAT(a, b) FLAG__CAT_(a, b)

#define FLAG__STATIC_ASSERT(cond, msg)                            \
    typedef char FLAG__CAT(flag__sa_, __LINE__)[(cond) ? 1 : -1]; \
    (void)sizeof(FLAG__CAT(flag__sa_, __LINE__))
#endif

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

FLAGDEF void flag_decl_int(Flags *flags,
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

FLAGDEF bool flag__parse_arg(void *val, FlagType type, char *arg)
{
    if ('-' == *arg && !(FLAG_INT == type || FLAG_DOUBLE == type)) return false;

    FLAG__STATIC_ASSERT(5 == _flag_count, "The amount of flag types has changed!");
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
        errno    = 0;
        double d = strtod(arg, &endptr);
        if (endptr == arg || 0 != *endptr || ERANGE == errno) return false;
        *(double *)val = d;
        return true;
    }
    break;
    case FLAG_INT: {
        char *endptr;
        errno       = 0;
        long long d = strtoll(arg, &endptr, 10);
        if (endptr == arg || 0 != *endptr || ERANGE == errno) return false;
        *(int64_t *)val = d;
        return true;
    }
    break;
    case FLAG_UINT: {
        char *endptr;
        errno                = 0;
        unsigned long long d = strtoull(arg, &endptr, 10);
        if (endptr == arg || 0 != *endptr || ERANGE == errno) return false;
        *(uint64_t *)val = d;
        return true;
    }
    break;
    default: assert(0); break;
    }

    return false;
}

FLAGDEF Flag *flag__find_long_flag(Flags *flags, const char *name, size_t name_len)
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

FLAGDEF int flag__apply_flag_val(Flags *flags, Flag *f, const char *flag_label, const char *attached_val)
{
    char *val_str = NULL;

    if (attached_val)
    {
        val_str = (char *)attached_val;
    }
    else if (FLAG_BOOL != f->type)
    {
        if (flags->argc <= 0)
        {
            fprintf(stderr, "ERROR: flag '%s' requires an argument\n", flag_label);
            return 1;
        }
        val_str = flag__shift_args(flags->argc, flags->argv);
    }
    else
    {
        if (flags->argc > 0 && (0 == strcmp(*flags->argv, "true") || 0 == strcmp(*flags->argv, "false")))
        {
            val_str = flag__shift_args(flags->argc, flags->argv);
        }
        else
        {
            *(bool *)f->user_ptr = true;
            f->seen              = true;
            return 0;
        }
    }

    if (!flag__parse_arg(f->user_ptr, f->type, val_str))
    {
        fprintf(stderr, "ERROR: invalid value '%s' for flag '%s'\n", val_str, flag_label);
        return 1;
    }

    f->seen = true;
    return 0;
}

FLAGDEF int flag_parse(Flags *flags, int argc, char **argv)
{
    flags->argc = argc;
    flags->argv = argv;
#ifndef FLAG_NO_PROGRAM_NAME
    flags->program_name = flag__shift_args(flags->argc, flags->argv);
#endif // FLAG_NO_PROGRAM_NAME

#ifndef FLAG_NO_HELP
    bool custom_help = false;
    flag__da_foreach(Flag, flag, flags)
    {
        if ((flag->short_flag && strcmp(flag->short_flag, "-h") == 0) ||
            (flag->long_flag && strcmp(flag->long_flag, "--help") == 0))
        {
            custom_help = true;
            break;
        }
    }

    if (!custom_help) flag_decl_bool(flags, &flags->help, "-h", "--help", false, "Display the help message and exit");
#endif // FLAG_NO_HELP

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

        FLAG__STATIC_ASSERT(5 == _flag_count, "The amount of flag types has changed!");
        switch (flag->type)
        {
        case FLAG_BOOL:   *(bool *)flag->user_ptr = flag->defaults.b; break;
        case FLAG_STRING: *(char **)flag->user_ptr = flag->defaults.s; break;
        case FLAG_DOUBLE: *(double *)flag->user_ptr = flag->defaults.d; break;
        case FLAG_INT:    *(int64_t *)flag->user_ptr = flag->defaults.i; break;
        case FLAG_UINT:   *(uint64_t *)flag->user_ptr = flag->defaults.u; break;
        default:          assert(0); break;
        }
    }

    while (flags->argc > 0)
    {
        char *flag            = flag__shift_args(flags->argc, flags->argv);
        const size_t flag_len = strlen(flag);
        if (flag_len < 2 || '-' != flag[0])
        {
            // stop at the first non flag
            flags->argc++;
            flags->argv--;
            break;
        }

        if ('-' == flag[1])
        {
            if (2 == flag_len)
            {
                // stop at the first --
                break;
            }

            const char *eq        = strchr(flag, '=');
            const size_t name_len = eq ? (size_t)(eq - flag) : strlen(flag);
            if (0 == name_len)
            {
                fprintf(stderr, "ERROR: malformed long flag '%s'\n", flag);
                return 1;
            }

            Flag *f = flag__find_long_flag(flags, flag, name_len);
            if (!f)
            {
                fprintf(stderr, "ERROR: unknown flag '%s'\n", flag);
                return 1;
            }

            const char *attached_val = eq ? (eq + 1) : NULL;
            if (0 != flag__apply_flag_val(flags, f, flag, attached_val))
            {
                return 1;
            }
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
            if (FLAG_BOOL == f->type)
            {
                *(bool *)f->user_ptr = true;
                f->seen              = true;
                continue;
            }

            char label[]             = {'-', c, 0};
            const char *attached_val = NULL;
            if ('=' == flag[i + 1])
            {
                attached_val = flag + i + 2;
            }
            else if (0 != flag[i + 1])
            {
                attached_val = flag + i + 1;
            }

            if (0 != flag__apply_flag_val(flags, f, label, attached_val))
            {
                return 1;
            }
            break;
        }
    }

#ifndef FLAG_NO_HELP
    if (!custom_help && flags->help)
    {
        flag_print_info(flags, "USAGE:");
        flag_free(flags);
        exit(0);
    }
#endif // FLAG_NO_HELP

    return 0;
}

FLAGDEF void flag_free(Flags *flags)
{
    flag__da_free(flags);
    memset(flags, 0, sizeof(*flags));
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

    puts(header ? header : "USAGE:");
    flag__da_foreach(Flag, flag, flags)
    {
        if (!flag->short_flag && !flag->long_flag) continue;

        const char *short_f = flag->short_flag ? flag->short_flag : "  ";
        const char *sep     = (flag->short_flag && flag->long_flag) ? ", " : "  ";
        const char *long_f  = flag->long_flag ? flag->long_flag : "";
        char *type_info     = NULL;
        FLAG__STATIC_ASSERT(5 == _flag_count, "The amount of flag types has changed!");
        switch (flag->type)
        {
        case FLAG_INT:    type_info = "<int>"; break;
        case FLAG_UINT:   type_info = "<uint>"; break;
        case FLAG_BOOL:   type_info = "[bool]"; break;
        case FLAG_STRING: type_info = "<str>"; break;
        case FLAG_DOUBLE: type_info = "<double>"; break;
        default:          assert(0);
        }
        printf("    %s%s%-*s %-6s    %s\n", short_f, sep, (int)max_len, long_f, type_info, flag->description);
    }

    printf("\n");
    printf(
        "    Syntax: <...> are mandatory arguments and [...] are optional. In the case of a `bool` this defaults to `true`\n");
}

FLAGDEF bool flag_seen(Flags *flags, void *user_ptr)
{
    flag__da_foreach(Flag, flag, flags)
    {
        if (flag->user_ptr == user_ptr) return flag->seen;
    }

    return false;
}

FLAGDEF void flag_rest_args(Flags *flags, int *argc, char ***argv)
{
    *argc = flags->argc;
    *argv = flags->argv;
}

#ifndef FLAG_NO_PROGRAM_NAME
FLAGDEF const char *flag_program_name(Flags *flags)
{
    return flags->program_name;
}
#endif // FLAG_NO_PROGRAM_NAME

#endif // FLAG_IMPLEMENTATION

#endif // FLAG_H_
