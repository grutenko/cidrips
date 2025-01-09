#include "version.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
        unsigned int addr;
        int cidr;
} addr_t;

typedef struct addr_list
{
        addr_t addr;
        size_t count;
        struct addr_list *next, *prev;
} addr_list_t;

addr_list_t *list_item_alloc()
{
        addr_list_t *p = malloc(sizeof(addr_list_t));
        if (p)
        {
                p->count = 0;
                p->next = (void *)0;
                p->prev = (void *)0;
        }
        return p;
}

static inline int64_t addr_raw_mask(addr_t *addr, int cidr)
{
        return addr->addr & (~0UL << (32 - cidr));
}

static inline int min(int a, int b)
{
        return a > b ? b : a;
}

static int addr_compare(addr_t *addr0, addr_t *addr1)
{
        int cidr;
        int64_t cmp;
        cidr = min(addr0->cidr, addr1->cidr);
        cmp = addr_raw_mask(addr0, cidr) - addr_raw_mask(addr1, cidr);
        if (cmp > 0)
        {
                return 1;
        }
        else if (cmp < 0)
        {
                return -1;
        }
        else if (addr0->cidr > addr1->cidr)
        {
                return -1;
        }
        else if (addr0->cidr < addr1->cidr)
        {
                return 1;
        }
        return 0;
}

static void list_insert_before(addr_list_t **head, addr_list_t **tail, addr_list_t *before_this, addr_list_t *item)
{
        if (before_this)
        {
                item->next = before_this;
                if (before_this->prev)
                {
                        before_this->prev->next = item;
                }
                else
                {
                        *head = item;
                }
                item->prev = before_this->prev;
                before_this->prev = item;
        }
        else
        {
                if (*tail)
                {
                        (*tail)->next = item;
                        item->prev = *tail;
                        *tail = item;
                }
                else
                {
                        *head = item;
                        *tail = item;
                }
        }
}

static int list_push_sorted(addr_list_t **head, addr_list_t **tail, addr_list_t *item)
{
        addr_list_t *p = *head;
        int rc;
        while (p)
        {
                rc = addr_compare(&(p->addr), &(item->addr));
                if (rc == 0)
                {
                        return 0;
                }
                else if (rc > 0)
                {
                        break;
                }
                p = p->next;
        }
        list_insert_before(head, tail, p, item);
        return 1;
}

void list_remove(addr_list_t **head, addr_list_t **tail, addr_list_t *item)
{
        if (item->prev)
        {
                item->prev->next = item->next;
        }
        else
        {
                *head = item->next;
        }
        if (item->next)
        {
                item->next->prev = item->prev;
        }
        else
        {
                *tail = item->next;
        }
        item->next = (void *)0;
        item->prev = (void *)0;
}

void list_free(addr_list_t **head, addr_list_t **tail)
{
        addr_list_t *p = *head, *t;
        while (p)
        {
                t = p->next;
                list_remove(head, tail, p);
                free(p);
                p = t;
        }
}

int list_copy(addr_list_t *head, addr_list_t *tail, addr_list_t **thead, addr_list_t **ttail)
{
        addr_list_t *p0 = head;
        addr_list_t *item;
        while (p0)
        {
                item = list_item_alloc();
                if (!item)
                {
                        return 0;
                }
                memmove(item, p0, sizeof(addr_list_t));
                item->prev = (void *)0;
                item->next = (void *)0;
                if (*thead)
                {
                        item->prev = *ttail;
                        (*ttail)->next = item;
                        *ttail = item;
                }
                else
                {
                        *thead = item;
                        *ttail = item;
                }
                p0 = p0->next;
        }
        return 1;
}

static int addr_parse_v4(char *data, size_t size, addr_t *addr)
{
        int rc, n;
        unsigned char a, b, c, d, cidr;
        rc = sscanf(data, "%hhu.%hhu.%hhu.%hhu%n", &a, &b, &c, &d, &n);
        if (rc < 4)
        {
                return 0;
        }
        data += n;
        size -= n;
        addr->cidr = 32;
        if (size > 0)
        {
                rc = sscanf(data, "/%hhu", &cidr);
                if (rc < 1 || cidr > 32)
                {
                        return 0;
                }
                addr->cidr = cidr;
        }
        addr->addr = (a << 24) | (b << 16) | (c << 8) | d;
        return 1;
}

static void addr_print_v4(FILE *o, addr_t *addr)
{
        fprintf(o, "%hhu.%hhu.%hhu.%hhu", addr->addr >> 24, (addr->addr >> 16) & 0xff, (addr->addr >> 8) & 0xff,
                addr->addr & 0xff);
        if (addr->cidr != 32)
        {
                fprintf(o, "/%hhu", addr->cidr);
        }
}

static inline size_t addr_v4_weight(int cidr)
{
        return 1ULL << (32 - cidr);
}

static size_t addr_start_col = 1, addr_start_row = 1, parse_col = 1, parse_row = 1;
static int parse_char;

enum
{
        PARSE_OK = 0,
        PARSE_EADDR = 1,
        PARSE_ESYMBOL = 2,
        PARSE_EMEM = 3,
        PARSE_EIO = 4
};

static int parse_input(FILE *o, addr_list_t **head, addr_list_t **tail)
{
        char b[64] = {0}, *p = b;
        char c;
        addr_list_t *item = list_item_alloc();
        if (!item)
        {
                return PARSE_EMEM;
        }
        parse_col = 1;
        parse_row = 1;
        int new_row_use_r = 0;
        while (1)
        {
                c = getc(o);
                parse_char = c;
                switch (c)
                {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                case '.':
                case '/':
                        if (p - b == 0)
                        {
                                addr_start_col = parse_col;
                                addr_start_row = parse_row;
                        }
                        *p = c;
                        p++;
                        if (b - p > 63)
                        {
                                return PARSE_EADDR;
                        }
                        break;
                case ' ':
                case '\t':
                case '\r':
                case '\n':
                case ',':
                case EOF:
                        if (c == '\r')
                        {
                                if (new_row_use_r)
                                {
                                        return PARSE_ESYMBOL;
                                }
                                new_row_use_r = 1;
                                parse_col = 1;
                                parse_row++;
                        }
                        else if (c == '\n')
                        {
                                if (new_row_use_r)
                                {
                                        new_row_use_r = 0;
                                }
                                else
                                {
                                        parse_row++;
                                }
                        }
                        if (p != b)
                        {
                                *p = '\0';
                                if (!addr_parse_v4(b, p - b, &(item->addr)))
                                {
                                        return PARSE_EADDR;
                                }
                                else
                                {
                                        item->count = addr_v4_weight(item->addr.cidr);
                                        if (list_push_sorted(head, tail, item))
                                        {
                                                item = list_item_alloc();
                                                if (!item)
                                                {
                                                        return PARSE_EMEM;
                                                }
                                        }
                                        else
                                        {
                                                // function detected copy of
                                                // this address. Skip
                                        }
                                }
                                p = b;
                        }
                        if (c == EOF)
                        {
                                if (!feof(o))
                                {
                                        return PARSE_EIO;
                                }
                                return PARSE_OK;
                        }
                        break;
                default:
                        return PARSE_ESYMBOL;
                }
                parse_col++;
        }
        return PARSE_OK;
}

typedef struct
{
        char input[256];
        char output[256];
        char prefix[256];
        char postfix[256];
        int mode;
        int count;
        int level;
        int help;
        int overwrite;
        int no_stats;
        int append;
        int cancel;
} args_t;

void cli_help(FILE *o)
{
        // clang-format off
        fprintf(o,
                "cidrips v%d.%d.%d.rev%s\nUtlity for compress any IP list groupping "
                "by CIDR mask.\n\n",
                cidrips_version_major, cidrips_version_minor, cidrips_version_patch,
                cidrips_git_rev);
        fprintf(o, "Usage:\n");
        fprintf(o, "\tcidrips -i[FILE]\n");
        fprintf(o, "\tcidrips -i[FILE] -o[FILE]\n");
        fprintf(o, "\tcidrips -mlevel [-l[level]] -i[FILE] -o[FILE]\n");
        fprintf(o, "\tcidrips -mcount [-c[count]] -i[FILE] -o[FILE]\n");
        fprintf(o, "\tcidrips -i[FILE] -p[PREFIX] -P[POSTFIX]\n\n");
        fprintf(o, "Arguments:\n");
        fprintf(o, "\t-i,--input    [FILE]           Path to file with input ips. (Use \n");
        fprintf(o, "\t                               --input - for stdin)\n");
        fprintf(o, "\t-o,--out      [FILE]           Path to file with output subnets. (Use \n");
        fprintf(o, "\t                               --input - for stdin)\n");
        fprintf(o, "\t-m,--mode     [level|count]    Use this method for generate \n");
        fprintf(o, "\t                               subnets: level >=0, count - maximum count\n");
        fprintf(o, "\t                               of result subnets.\n");
        fprintf(o, "\t                               [Default: level]\n");
        fprintf(o, "\t-l,--level    [LEVEL]          Comression level.  [Default: 0]\n");
        fprintf(o, "\t-c,--count    [COUNT]          Maxmimum count of result subnets.\n");
        fprintf(o, "\t                               [Default: not specifie]\n");
        fprintf(o, "\t-p,--prefix   [prefix]         Prefix for generated subnet in output.\n");
        fprintf(o, "\t                               [Default: \"\"]\n");
        fprintf(o, "\t-P,--postfix  [postfix]        Postfix for generated subnet in output.\n");
        fprintf(o, "\t                               [Default: \"\\n\"\n");
        fprintf(o, "\t-O, --overwrite                Overwrite output file if not empty\n");
        fprintf(o, "\t-A, --append                   Append into output file.\n");
        fprintf(o, "\t-C, --cancel                   Cancel if output file is not empty.\n");
        fprintf(o, "\t    --no-stats                 No print any statistics\n");
        // clang-format on
}

#define MODE_UNKNOWN 0
#define MODE_LEVEL 1
#define MODE_COUNT 2

#define ARG_OPTIONAL 0x1
#define ARG_NO_VALUE 0x2

typedef int(arg_handler_cb)(const char *arg_val, args_t *cli_args);

static int arg_input(const char *arg_val, args_t *cli_args)
{
        if (arg_val == (void *)0)
        {
                return 0;
        }
        if (strlen(arg_val) > 255)
        {
                fprintf(stderr, "--input: argument too long.\n");
                return 0;
        }
        strcpy(cli_args->input, arg_val);
        return 1;
}

static int arg_output(const char *arg_val, args_t *cli_args)
{
        if (arg_val == (void *)0)
        {
                cli_args->output[0] = 0;
        }
        else if (strlen(arg_val) > 255)
        {
                fprintf(stderr, "--output: argument too long.\n");
                return 0;
        }
        else
        {
                strcpy(cli_args->output, arg_val);
                return 1;
        }
        return 1;
}

static int arg_mode(const char *arg_val, args_t *cli_args)
{
        if (arg_val == (void *)0)
        {
                cli_args->mode = MODE_LEVEL;
        }
        else if (strcmp(arg_val, "level") == 0)
        {
                cli_args->mode = MODE_LEVEL;
        }
        else if (strcmp(arg_val, "count") == 0)
        {
                cli_args->mode = MODE_COUNT;
        }
        else
        {
                fprintf(stderr,
                        "--mode: invalid value, support only level or count. "
                        "got: \"%s\"\n",
                        arg_val);
                return 0;
        }
        return 1;
}

static int arg_level(const char *arg_val, args_t *cli_args)
{
        if (cli_args->mode != MODE_UNKNOWN && cli_args->mode != MODE_LEVEL)
        {
                fprintf(stderr, "--level: Unexpected for this mode.\n");
                return 0;
        }
        if (arg_val == (void *)0)
        {
                cli_args->level = 0;
        }
        else if (arg_val[0] >= '0' && arg_val[0] <= '9')
        {
                cli_args->level = atoi(arg_val);
        }
        else
        {
                fprintf(stderr, "--level: invalid value, non-negative numbers.\n");
                return 0;
        }
        return 1;
}

static int arg_count(const char *arg_val, args_t *cli_args)
{
        if (cli_args->mode != MODE_UNKNOWN && cli_args->mode == MODE_LEVEL)
        {
                fprintf(stderr, "--count: Unexpected for this mode.\n");
                return 0;
        }
        if (arg_val == (void *)0)
        {
                cli_args->count = 1;
        }
        else if (arg_val[0] > '0' && arg_val[0] <= '9')
        {
                cli_args->count = atoi(arg_val);
        }
        else
        {
                fprintf(stderr, "--count: invalid value, positive numbers. got %s\n", arg_val);
                return 0;
        }
        return 1;
}

void strcpy_escaped(char *s0, const char *s1)
{
        int escape = 0;
        char *p = (char *)s1;
        char *out = s0;
        while (*p)
        {
                if (!escape)
                {
                        if (*p == '\\')
                        {
                                escape = 1;
                        }
                        else
                        {
                                *out = *p;
                                out++;
                        }
                }
                else
                {
                        escape = 0;
                        if (*p == 'n')
                        {
                                *out = '\n';
                        }
                        else if (*p == 'r')
                        {
                                *out = '\r';
                        }
                        else if (*p == 't')
                        {
                                *out = '\t';
                        }
                        else
                        {
                                *out = '\\';
                                out++;
                                *out = *p;
                        }
                        out++;
                }
                p++;
        }
        *out = '\0';
}

static int arg_prefix(const char *arg_val, args_t *cli_args)
{
        if (!arg_val)
        {
                cli_args->prefix[0] = '\0';
                return 1;
        }

        if (strlen(arg_val) > 255)
        {
                fprintf(stderr, "--prefix: argument too long.\n");
                return 0;
        }
        strcpy_escaped(cli_args->prefix, arg_val);
        return 1;
}

static int arg_postfix(const char *arg_val, args_t *cli_args)
{
        if (!arg_val)
        {
                cli_args->prefix[0] = '\n';
                cli_args->prefix[1] = '\0';
                return 1;
        }

        if (strlen(arg_val) > 255)
        {
                fprintf(stderr, "--postfix: argument too long.\n");
                return 0;
        }
        strcpy_escaped(cli_args->postfix, arg_val);
        return 1;
}

static int arg_overwrite(const char *arg_val, args_t *cli_args)
{
        cli_args->overwrite = 1;
        return 1;
}

static int arg_append(const char *arg_val, args_t *cli_args)
{
        cli_args->append = 1;
        return 1;
}

static int arg_cancel(const char *arg_val, args_t *cli_args)
{
        cli_args->cancel = 1;
        return 1;
}

static int arg_no_stats(const char *arg_val, args_t *cli_args)
{
        cli_args->no_stats = 1;
        return 1;
}

static int arg_help(const char *arg_val, args_t *cli_args)
{
        cli_args->help = 1;
        return 1;
}

static struct argtab
{
        int arg_index;
        const char arg_short;
        const char *arg_long;
        int arg_flags;
        const char *arg_default;
        const char *arg_description;
        arg_handler_cb *arg_handler;
} _argtab[] = {
    // clang-format off
    {0, 'h', "help", ARG_OPTIONAL | ARG_NO_VALUE, 0, "Show help.", arg_help},
    {1, 'i', "input", 0, 0, "Path to input file with ip addresses.", arg_input},
    {2, 'o', "output", ARG_OPTIONAL, 0, "Path to output file.", arg_output},
    {3, 'm', "mode", ARG_OPTIONAL, "level", "Compression mode.", arg_mode},
    {4, 'l', "level", ARG_OPTIONAL, "0", "Compress level. Required for --mode=level.", arg_level},
    {5, 'c', "count", ARG_OPTIONAL, 0, "Max count of output. Required for --mode=count.", arg_count},
    {6, 'p', "prefix", ARG_OPTIONAL, "", "Prefix for ouput ip address.", arg_prefix},
    {7, 'P', "postfix", ARG_OPTIONAL, "\n", "Postfix for ouput ip address", arg_postfix},
    {8, 'O', "overwrite", ARG_OPTIONAL | ARG_NO_VALUE, 0, "Owerwrite ouput is not empty.", arg_overwrite},
    {9, 's', "no-stats", ARG_OPTIONAL | ARG_NO_VALUE, 0, "Dont print any statistic info.", arg_no_stats},
    {10, 'A', "append", ARG_OPTIONAL | ARG_NO_VALUE, 0, "Append file if not empty", arg_append},
    {11, 'C', "cancel", ARG_OPTIONAL | ARG_NO_VALUE, 0, "Cancel if output file is not empty.", arg_cancel}};
// clang-format on
static int _argtab_size = 12;

static int cli_arg_val_contain(char arg_short, const char *arg_long, const char *argv)
{
        // clang-format off
  if (argv[0] == '-' && argv[1] == arg_short && strlen(argv) == 2)
    return 0;
  if (argv[0] == '-' && argv[1] == '-' && strstr(argv, arg_long) == argv + 2 &&
      strlen(argv) == strlen(arg_long) + 2 && argv[strlen(arg_long) + 2] != '=')
    return 0;
        // clang-format on
        return 1;
}

static char *cli_arg_val(char arg_short, const char *arg_long, const char *argv)
{
        char *p, *pp;
        if (argv[0] == '-' && argv[1] == arg_short)
                p = (char *)argv + 2;
        if (argv[0] == '-' && argv[1] == '-' && strstr(argv, arg_long) == argv + 2)
                p = (char *)argv + strlen(arg_long) + 2;
        if (*p == '\0')
        {
                return (char *)0;
        }
        if (*p == '=')
        {
                p++;
        }
        if (*p == '"')
        {
                p++;
                pp = strchr(p, '"');
                if (pp)
                {
                        *pp = 0;
                }
        }
        if (*p == '\0')
        {
                return (void *)0;
        }
        return p;
}

struct argtab *find_arg(const char *argv)
{
        int i;
        for (i = 0; i < _argtab_size; i++)
        {
                if (argv[0] == '-' && argv[1] == _argtab[i].arg_short)
                        return &_argtab[i];
                if (argv[0] == '-' && strstr(argv, _argtab[i].arg_long) == argv + 2)
                        return &_argtab[i];
        }
        return (void *)0;
}

int cli_parse(int argc, const char **argv, args_t *args)
{
        int i, rc;
        struct argtab *argtab;
        int arg_passed[64] = {0};
        for (i = 1; i < argc; i++)
        {
                argtab = find_arg(argv[i]);
                if (argtab)
                {
                        arg_passed[argtab->arg_index] = 1;
                        if (cli_arg_val_contain(argtab->arg_short, argtab->arg_long, argv[i]))
                        {
                                if (argtab->arg_flags & ARG_NO_VALUE)
                                {
                                        fprintf(stderr, "--%s: Unexpected value.\n", argtab->arg_long);
                                        return 0;
                                }
                                else
                                {
                                        rc = argtab->arg_handler(
                                            cli_arg_val(argtab->arg_short, argtab->arg_long, argv[i]), args);
                                        if (!rc)
                                        {
                                                return 0;
                                        }
                                }
                        }
                        else
                        {
                                if (i == argc - 1)
                                {
                                        if (argtab->arg_flags & ARG_NO_VALUE)
                                        {
                                                rc = argtab->arg_handler(argv[i], args);
                                                if (!rc)
                                                {
                                                        return 0;
                                                }
                                        }
                                        else
                                        {
                                                fprintf(stderr,
                                                        "--%s: Expected "
                                                        "argument value.\n",
                                                        argtab->arg_long);
                                                return 0;
                                        }
                                }
                                else
                                {
                                        if (argtab->arg_flags & ARG_NO_VALUE)
                                        {
                                                rc = argtab->arg_handler(argv[i], args);
                                                if (!rc)
                                                {
                                                        return 0;
                                                }
                                        }
                                        else
                                        {
                                                i++;
                                                rc = argtab->arg_handler(argv[i], args);
                                                if (!rc)
                                                {
                                                        return 0;
                                                }
                                        }
                                }
                        }
                        if (argtab->arg_short == 'h')
                        {
                                return 1;
                        }
                }
                else
                {
                        fprintf(stderr, "%s: Unknown argument\n", argv[i]);
                        return 0;
                }
        }

        for (i = 0; i < _argtab_size; i++)
        {
                if (arg_passed[i] == 0 && !(_argtab[i].arg_flags & ARG_OPTIONAL))
                {
                        fprintf(stderr, "--%s: required.\n", _argtab[i].arg_long);
                        return 0;
                }
        }
        return 1;
}

#define REASON_REWRITE 0
#define REASON_APPEND 1
#define REASON_STDOUT 2
#define REASON_CANCEL 3
#define REASON_IO_ERROR 4

static int ask_output_file_reason(char *file)
{
        int rc;
        while (1)
        {
                rc = printf("File %s is not empty. Rewrite? [y[es]/n[o]/a[ppend]] ", file);
                if (rc == -1)
                {
                        return REASON_IO_ERROR;
                }
                rc = getc(stdin);
                if (rc == EOF)
                {
                        return REASON_IO_ERROR;
                }
                if (rc == 'y')
                {
                        return REASON_REWRITE;
                }
                else if (rc == 'n')
                {
                        return REASON_CANCEL;
                }
                else if (rc == 'a')
                {
                        return REASON_APPEND;
                }
                else
                {
                        // nothing to do invalid input
                }
        }
        return REASON_CANCEL;
}

static struct compress_stats
{
        size_t coverage;
        size_t source_count;
} compress_stats;

static int compress(addr_list_t **head, addr_list_t **tail, int level)
{
        addr_list_t *p, *ph, *pt, *pnext;
        size_t sum = 0, ips_count = 0;
        int i;
        int count;
        for (i = 31; i >= 0; i--)
        {
                p = *head;
                count = 0;
                compress_stats.coverage = 0;
                compress_stats.source_count = 0;
                while (p)
                {
                        ph = p->next;
                        pt = p->next;
                        sum = p->count;
                        ips_count = 0;
                        while (pt)
                        {
                                if (addr_raw_mask(&(p->addr), i) != addr_raw_mask(&pt->addr, i))
                                {
                                        break;
                                }
                                sum += pt->count;
                                ips_count++;
                                pt = pt->next;
                        }
                        if (ips_count > 0)
                        {
                                if (sum >= (addr_v4_weight(i) >> level))
                                {
                                        p->count = sum;
                                        p->addr.cidr = i;
                                        p->addr.addr = (uint32_t)addr_raw_mask(&(p->addr), i);
                                        do
                                        {
                                                pnext = ph->next;
                                                list_remove(head, tail, ph);
                                                ph = pnext;
                                        } while (ph != pt);
                                }
                        }
                        compress_stats.coverage += addr_v4_weight(p->addr.cidr);
                        compress_stats.source_count += p->count;
                        count++;
                        p = p->next;
                }
        }
        return count;
}

int main(int argc, const char **argv)
{
        FILE *o;
        int rc;
        args_t args = {0};
        args.postfix[0] = '\n';
        args.postfix[1] = '\0';
        rc = cli_parse(argc, argv, &args);
        if (!rc)
        {
                return EXIT_FAILURE;
        }

        if (args.help)
        {
                cli_help(stdout);
                return EXIT_SUCCESS;
        }

        if (strcmp(args.input, "-") == 0)
        {
                o = stdin;
        }
        else
        {
                o = fopen(args.input, "r");
                if (!o)
                {
                        fprintf(stderr, "Cannot open file: %s %s\n", args.input, strerror(errno));
                        return EXIT_FAILURE;
                }
        }

        addr_list_t *head = (void *)0, *tail = (void *)0;
        rc = parse_input(o, &head, &tail);
        if (rc == PARSE_EADDR)
        {
                fprintf(stderr, "Invalid address at %ld:%ld.\n", addr_start_row, addr_start_col);
                return EXIT_FAILURE;
        }
        else if (rc == PARSE_EIO)
        {
                list_free(&head, &tail);
                fprintf(stderr, "I/O error: %s.\n", strerror(errno));
                return EXIT_FAILURE;
        }
        else if (rc == PARSE_EMEM)
        {
                list_free(&head, &tail);
                fprintf(stderr, "Cannot allocate memory.\n");
                return EXIT_FAILURE;
        }
        else if (rc == PARSE_ESYMBOL)
        {
                list_free(&head, &tail);
                fprintf(stderr, "Unexpected symbol \"%c\" at %ld:%ld.\n", parse_char, parse_row, parse_row);
                return EXIT_FAILURE;
        }

        int i, count;
        addr_list_t *thead = (void *)0, *ttail = (void *)0;

        if (args.mode == MODE_LEVEL)
        {
                count = compress(&head, &tail, args.level);
        }
        else
        {
                for (i = 0; i <= 32; i++)
                {
                        if (!list_copy(head, tail, &thead, &ttail))
                        {
                                fprintf(stderr, "Memory allocation error.\n");
                                return EXIT_FAILURE;
                        }
                        count = compress(&thead, &ttail, i);
                        if (count <= args.count)
                        {
                                list_free(&head, &tail);
                                head = thead;
                                tail = ttail;
                                break;
                        }
                        list_free(&thead, &ttail);
                }
        }

        if (!args.no_stats)
        {
                printf("coverage=%ld, source=%ld, falsely_covered=%lf%%; "
                       "result=%d, "
                       "compress=%lf%%\n",
                       compress_stats.coverage, compress_stats.source_count,
                       100.00f - ((double)compress_stats.source_count / compress_stats.coverage * 100.00f), count,
                       100.00f - ((double)count / compress_stats.source_count * 100.00f));
        }

        int output_file_reason;

        if (strcmp(args.output, "-") == 0)
        {
                output_file_reason = REASON_STDOUT;
        }
        else
        {
                o = fopen(args.output, "r");
                if (o)
                {
                        fseek(o, 0, SEEK_END);
                        if (ftell(o) > 0)
                        {
                                if (strcmp(args.input, "-") == 0)
                                {
                                        if (args.overwrite)
                                        {
                                                output_file_reason = REASON_REWRITE;
                                        }
                                        else if (args.append)
                                        {
                                                output_file_reason = REASON_APPEND;
                                        }
                                        else
                                        {
                                                fprintf(stderr, "Output file is not "
                                                                "empty. Cancelled.\n");
                                                output_file_reason = REASON_CANCEL;
                                        }
                                }
                                else
                                {
                                        if (args.overwrite)
                                        {
                                                output_file_reason = REASON_REWRITE;
                                        }
                                        else if (args.append)
                                        {
                                                output_file_reason = REASON_APPEND;
                                        }
                                        else if (args.cancel)
                                        {
                                                output_file_reason = REASON_CANCEL;
                                        }
                                        else
                                        {
                                                output_file_reason = ask_output_file_reason(args.output);
                                        }
                                }
                        }
                        else
                        {
                                output_file_reason = REASON_REWRITE;
                        }
                }
                else
                {
                        output_file_reason = REASON_REWRITE;
                }
        }

        if (output_file_reason == REASON_REWRITE)
        {
                o = fopen(args.output, "w");
        }
        else if (output_file_reason == REASON_APPEND)
        {
                o = fopen(args.output, "a");
        }
        else if (output_file_reason == REASON_STDOUT)
        {
                o = stdout;
        }
        else if (output_file_reason == REASON_CANCEL)
        {
                list_free(&head, &tail);
                fprintf(stdout, "Cancelled by user.\n");
                return EXIT_SUCCESS;
        }

        if (!o)
        {
                list_free(&head, &tail);
                fprintf(stderr, "Cannot open file: %s %s\n", args.input, strerror(errno));
                return EXIT_FAILURE;
        }

        addr_list_t *p = head;
        while (p)
        {
                rc = fprintf(o, "%s", args.prefix);
                if (rc < 0)
                {
                        fprintf(stderr, "I/O error: %s", strerror(errno));
                        list_free(&head, &tail);
                        return EXIT_FAILURE;
                }
                addr_print_v4(o, &p->addr);
                rc = fprintf(o, "%s", args.postfix);
                if (rc < 0)
                {
                        fprintf(stderr, "I/O error: %s", strerror(errno));
                        list_free(&head, &tail);
                        return EXIT_FAILURE;
                }
                p = p->next;
        }

        list_free(&head, &tail);
        return EXIT_SUCCESS;
}