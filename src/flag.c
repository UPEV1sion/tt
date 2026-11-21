#include <assert.h>
#include <string.h>

#include "flag.h"
// #include "ht.h"

#define ALPHABET_SIZE 256
#define shift_args(argc, argv)((argv)--, *(argv)++)

static Flag new_flag_bool(
        bool *val,
        const char *short_flag, 
        const char *long_flag, 
        bool default_val, 
        const char *description)
{
    return (Flag) {
        .type        = FLAG_BOOL,
        .user_ptr    = val,
        .short_flag  = short_flag,
        .long_flag   = long_flag,
        .defaults.b  = default_val,
        .description = description,
    };
}

static Flag new_flag_str(
        char **val,
        const char *short_flag, 
        const char *long_flag, 
        char *default_val, 
        const char *description)
{
    return (Flag) {
        .type        = FLAG_STRING,
        .user_ptr    = val,
        .short_flag  = short_flag,
        .long_flag   = long_flag,
        .defaults.s  = default_val,
        .description = description,
    };
}

static Flag new_flag_int(
        int64_t *val,
        const char *short_flag, 
        const char *long_flag, 
        int64_t default_val, 
        const char *description)
{
    return (Flag) {
        .type        = FLAG_INT,
        .user_ptr    = val,
        .short_flag  = short_flag,
        .long_flag   = long_flag,
        .defaults.i  = default_val,
        .description = description,
    };
}

static Flag new_flag_uint(
        uint64_t *val,
        const char *short_flag, 
        const char *long_flag, 
        int64_t default_val, 
        const char *description)
{
    return (Flag) {
        .type        = FLAG_UINT,
        .user_ptr    = val,
        .short_flag  = short_flag,
        .long_flag   = long_flag,
        .defaults.u  = default_val,
        .description = description,
    };
}

static Flag new_flag_double(
        double *val,
        const char *short_flag, 
        const char *long_flag, 
        int64_t default_val, 
        const char *description)
{
    return (Flag) {
        .type        = FLAG_UINT,
        .user_ptr    = val,
        .short_flag  = short_flag,
        .long_flag   = long_flag,
        .defaults.d  = default_val,
        .description = description,
    };
}

void flag_decl_bool(Flags *flags, bool *val, const char *short_flag, const char *long_flag, bool default_val, const char *description)
{
    const Flag f = new_flag_bool(val, short_flag, long_flag, default_val, description);
    da_append(flags, f);
}

void flag_decl_str(Flags *flags, char **val, const char *short_flag, const char *long_flag, char *default_val, const char *description)
{
    const Flag f = new_flag_str(val, short_flag, long_flag, default_val, description);
    da_append(flags, f);
}

void flag_decl_double(Flags *flags, double *val, const char *short_flag, const char *long_flag, double default_val, const char *description)
{
    const Flag f = new_flag_double(val, short_flag, long_flag, default_val, description);
    da_append(flags, f);
}

void flag_decl_int(Flags *flags, int64_t *val, const char *short_flag, const char *long_flag, int64_t default_val, const char *description)
{
    const Flag f = new_flag_int(val, short_flag, long_flag, default_val, description);
    da_append(flags, f);
}

void flag_decl_uint(Flags *flags, uint64_t *val, const char *short_flag, const char *long_flag, uint64_t default_val, const char *description)
{
    const Flag f = new_flag_uint(val, short_flag, long_flag, default_val, description);
    da_append(flags, f);
}

static bool peek_arg(void *val, FlagType type, int argc, char **argv)
{
    if(0 == argc) return false;
    if('-' == *argv[1]) return false;

    switch(type)
    {
        case FLAG_BOOL: {
            bool b = 0 == strcmp(argv[1], "true");
            if(!(b || 0 == strcmp(argv[1], "false"))) return false;
            *(bool*)val = b;
            return true;
        } break;
        case FLAG_STRING: {
            *(char**)val = argv[1];  
            return true;
        } break;
        case FLAG_DOUBLE: {
            char *endptr;
            double d = strtod(argv[1], &endptr);
            if(0 != *endptr) return false;
            *(double*)val = d;
        } break;
        case FLAG_INT: {
            char *endptr;
            long long d = strtoll(argv[1], &endptr, 10);
            if(0 != *endptr) return false;
            *(int64_t*)val = d;
        } break;
        case FLAG_UINT: {
            char *endptr;
            unsigned long long d = strtoull(argv[1], &endptr, 10);
            if(0 != *endptr) return false;
            *(uint64_t*)val = d;
        } break;
    }

    return false;
}

int flag_parse(Flags *flags, int argc, char **argv)
{
    Flag *short_flags[ALPHABET_SIZE] = {0};

    da_foreach(Flag, flag, flags)
    {
        if(flag->short_flag) 
        {
            assert(strlen(flag->short_flag) == 2 && '-' == flag->short_flag[0] && 0 == flag->short_flag[2]);
            short_flags[(unsigned char) flag->short_flag[1]] = flag;
        }
        if(flag->long_flag) assert(strlen(flag->short_flag) >= 2 && '-' == flag->short_flag[0] && '-' == flag->short_flag[1]);

        switch(flag->type)
        {
            case FLAG_BOOL: {
                *(bool*)flag->user_ptr = flag->defaults.b;
            } break;
            case FLAG_STRING: {
                *(char**)flag->user_ptr = flag->defaults.s;
            } break;
            case FLAG_DOUBLE: {
                *(double*)flag->user_ptr = flag->defaults.d;
            } break;
            case FLAG_INT: {
                *(int*)flag->user_ptr = flag->defaults.i;
            } break;
            case FLAG_UINT: {
                *(int*)flag->user_ptr = flag->defaults.u;
            } break;
        }
    }

    while(argc > 0)
    {
        const char *flag = shift_args(argc, argv);
        if('-' != flag[0])
        {
            fprintf(stderr, "ERROR: invalid flag format '%s'\n", flag);
            return 1;
        }

        if('-' == flag[1])
        {
            // TODO long flag
        }

        const size_t flag_len = strlen(flag);
        if(flag_len < 2) return 1;
        for(size_t i = 1; i < flag_len; ++i)
        {
            const char c = flag[i];
            Flag *f = short_flags[(unsigned char) c];
            if(!f)
            {
                fprintf(stderr, "ERROR: invalid flag '-%c'\n", c);
                return 1;
            }

            if(2 == flag_len)
            {
                // TODO error
                switch(f->type)
                {
                    case FLAG_BOOL: {
                        bool val;
                        if(!peek_arg(&val, FLAG_BOOL, argc, argv)) val = true;
                        *(bool*)f->user_ptr = val;
                    } break;
                    case FLAG_STRING: {
                        char *val;
                        if(!peek_arg(&val, FLAG_STRING, argc, argv)) return 1;
                        *(char**)f->user_ptr = val;
                    } break;
                    case FLAG_DOUBLE: {
                        double val;
                        if(!peek_arg(&val, FLAG_DOUBLE, argc, argv)) return 1;
                        *(double*)f->user_ptr = val;
                    } break;
                    case FLAG_INT: {
                        int64_t val;
                        if(!peek_arg(&val, FLAG_INT, argc, argv)) return 1;
                        *(int64_t*)f->user_ptr = val;
                    } break;
                    case FLAG_UINT: {
                        uint64_t val;
                        if(!peek_arg(&val, FLAG_UINT, argc, argv)) return 1;
                        *(uint64_t*)f->user_ptr = val;
                    } break;
                }
            }
            else if(1 == i && '=' == flag[2])
            {
                
            }
            else if(FLAG_BOOL != f->type)
            {
                // TODO error
            }
        }
    }

    return 0;
}

void flag_free(Flags *flags)
{
    da_free(flags);
}
