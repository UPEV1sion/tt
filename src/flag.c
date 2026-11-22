#include <assert.h>
#include <string.h>

#include "flag.h"

#define ALPHABET_SIZE 256
#define shift_args(argc, argv)((argc)--, *(argv)++)

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

static bool parse_arg(void *val, FlagType type, char *arg)
{
    if('-' == *arg) return false;

    switch(type)
    {
        case FLAG_BOOL: {
            bool b = 0 == strcmp(arg, "true");
            if(!(b || 0 == strcmp(arg, "false"))) return false;
            *(bool*)val = b;
            return true;
        } break;
        case FLAG_STRING: {
            *(char**)val = arg;  
            return true;
        } break;
        case FLAG_DOUBLE: {
            char *endptr;
            double d = strtod(arg, &endptr);
            if(0 != *endptr) return false;
            *(double*)val = d;
        } break;
        case FLAG_INT: {
            char *endptr;
            long long d = strtoll(arg, &endptr, 10);
            if(0 != *endptr) return false;
            *(int64_t*)val = d;
        } break;
        case FLAG_UINT: {
            char *endptr;
            unsigned long long d = strtoull(arg, &endptr, 10);
            if(0 != *endptr) return false;
            *(uint64_t*)val = d;
        } break;
    }

    return false;
}

static Flag* find_long_flag(Flags *flags, const char *name, size_t name_len)
{
    da_foreach(Flag, flag, flags)
    {
        if (!flag->long_flag) continue;
        const char *long_name = flag->long_flag + 2;
        if (strlen(long_name) == name_len && strncmp(long_name, name, name_len) == 0)
        {
            return flag;
        }
    }
    return NULL;
}

static int apply_flag_val(Flag *f, const char *flag_label, int *argc, char ***argv, const char *attached_val)
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
        val_str = shift_args(*argc, *argv);
    }
    else
    {
        if (*argc > 0 && (0 == strcmp(**argv, "true") || 0 == strcmp(**argv, "false")))
        {
            val_str = shift_args(*argc, *argv);
        }
        else
        {
            *(bool *)f->user_ptr = true;
            return 0;
        }
    }

    if (!parse_arg(f->user_ptr, f->type, val_str))
    {
        fprintf(stderr, "ERROR: invalid value '%s' for flag '%s'\n", val_str, flag_label);
        return 1;
    }

    return 0;
}

int flag_parse(Flags *flags, int argc, char **argv)
{
    shift_args(argc, argv); // remove program name. TODO enforce this from the user?

    Flag *short_flags[ALPHABET_SIZE] = {0};

    da_foreach(Flag, flag, flags)
    {
        if(flag->short_flag) 
        {
            assert(strlen(flag->short_flag) == 2 && '-' == flag->short_flag[0] && 0 == flag->short_flag[2]);
            short_flags[(unsigned char) flag->short_flag[1]] = flag;
        }
        if(flag->long_flag) assert(strlen(flag->long_flag) >= 2 && '-' == flag->long_flag[0] && '-' == flag->long_flag[1]);

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
        char *flag = shift_args(argc, argv);
        const size_t flag_len = strlen(flag);
        if(flag_len < 2)
        {
            fprintf(stderr, "ERROR: malformed flag '%s'\n", flag);
            return 1;
        }

        if('-' == flag[1])
        {
            if(2 == flag_len)
            {
                fprintf(stderr, "ERROR: standalone '--' is not supported\n");
                return 1;
            }

            const char *eq = strchr(flag, '=');
            const size_t name_len = eq ? (size_t) (eq - flag) : strlen(flag);
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

        if(2 == flag_len || '=' == flag[2])
        {
            Flag *f = short_flags[(unsigned char) flag[1]];
            if (!f) 
            {
                fprintf(stderr, "ERROR: unknown flag '%s'\n", flag);
                return 1;
            }

            char *val_str = NULL;
            if('=' == flag[2])
            {
                val_str = flag + 3;
            }
            else if(FLAG_BOOL != f->type)
            {
                if(argc <= 0)
                {
                    fprintf(stderr, "ERROR: flag '%s' requires an argument\n", flag);
                    return 1;
                }
                val_str = shift_args(argc, argv);
            }
            else
            {
                if(argc > 0 && (0 == strcmp(*argv, "true") || 0 == strcmp(*argv, "false")))
                {
                    val_str = shift_args(argc, argv);
                }
                else
                {
                    *(bool*)f->user_ptr = true;
                    continue;
                }
            }

            if(!parse_arg(f->user_ptr, f->type, val_str))
            {
                fprintf(stderr, "ERROR: invalid value '%s' for flag '%s'\n", val_str, flag);
                return 1;
            }
            continue;
        }

        for(size_t i = 0; i < flag_len; ++i)
        {
            const char c = flag[i];
            Flag *f = short_flags[(unsigned char) c];
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
        }
    }

    return 0;
}

void flag_free(Flags *flags)
{
    da_free(flags);
}

void flag_print_info(Flags *flags, const char *header)
{
    size_t max_len = 0;
    da_foreach(Flag, flag, flags)
    {
        if(flag->long_flag)
        {
            const size_t len = strlen(flag->long_flag);
            if(len > max_len) max_len = len;
        }
    }

    puts(header);
    da_foreach(Flag, flag, flags)
    {
        if(flag->short_flag && flag->long_flag)
        {
            printf("    %s, %-*s    %s\n", flag->short_flag, (int) max_len, flag->long_flag, flag->description);
        }
        else if (flag->short_flag)
        {
            printf("    %s  %-*s    %s\n", flag->short_flag, (int) max_len, "", flag->description);
        }
        else if (flag->long_flag)
        {
            printf("        %-*s    %s\n", (int)max_len, flag->long_flag, flag->description);
        }
    }
}
