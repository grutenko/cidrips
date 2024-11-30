#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum
{
    SEP_COMMA = 0,
    SEP_LINE = 1
};

enum
{
    CIDR_24 = 0,
    CIDR_16 = 1,
    CIDR_8 = 2
};

enum
{
    WALK_LEFT = 0,
    WALK_RIGHT = 1,
    WALK_END = 2
};

static void print_help(FILE *stream)
{
    fprintf(stream, "cidrips. Utlity for group list of ips by the CIDR subnets.\n\n");
    fprintf(stream, "Usage: cidrips.exe --comma-separated < ips.txt > subnodes.txt\n\n");
    fprintf(stream, "Arguments:\n");
    fprintf(stream, "-c,--comma-separated        Use comma separated input and output.\n");
    fprintf(stream, "-l,--line-separated         Use line separated input and output.\n");
    fprintf(stream, "--cidr-24                   Use CIDR 24 0.0.0.0/24\n");
    fprintf(stream, "--cidr-16                   Use CIDR 16 0.0.0.0/16\n");
    fprintf(stream, "--cidr-8                    Use CIDR 8 0.0.0.0/8\n");
    fprintf(stream, "--help\n");
}

static struct node
{
    unsigned long addr;
    int walk;
    struct node *p, *left, *right;
} *subnet_head = NULL;

static int find_subnet(int subnet)
{
    struct node **p = &subnet_head, *pp = *p;
    while(*p)
    {
        pp = *p;
        if(subnet > (*p)->addr) p = &((*p)->right);
        else if(subnet < (*p)->addr) p = &((*p)->left);
        else return 1;
    }
    // create new node
    struct node *o = malloc(sizeof(struct node));
    if(o)
    {
        o->addr = subnet;
        o->walk = WALK_LEFT;
        o->left = o->right = NULL;
        o->p = pp;
        *p = o;
    }
    return !!o;
}

static void free_nodes()
{

}

int main(int argc, char **argv)
{
    int __sep = SEP_LINE;
    int __cidr = CIDR_24;
    int __help = 0;
    int i;
    for(i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--comma-separated") == 0 || strcmp(argv[i], "-c") == 0) __sep = SEP_COMMA;
        else if(strcmp(argv[i], "--line-separated") == 0 || strcmp(argv[i], "-l") == 0)  __sep = SEP_LINE;
        else if(strcmp(argv[i], "--cidr-24") == 0)  __cidr = CIDR_24;
        else if(strcmp(argv[i], "--cidr-16") == 0) __cidr = CIDR_16;
        else if(strcmp(argv[i], "--cidr-8") == 0) __cidr = CIDR_8;
        else if(strcmp(argv[i], "--help") == 0) __help = 1;
        else goto __argument_error;
    }

    if(__help)
    {
        print_help(stdout);
        return EXIT_SUCCESS;
    }

    int __mask;
    switch(__cidr)
    {
        case CIDR_24: __mask = 0xffffff00; break;
        case CIDR_16: __mask = 0xffff0000; break;
        case CIDR_8:  __mask = 0xff000000; break;
    }

    char addr[128];
    char *paddr = addr;
    int rc;
    unsigned int _a, _b, _c, _d;
    char c;
    int __row = 1, __col = 1;
    while(!feof(stdin))
    {
        c = getc(stdin);
        switch(c)
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
            if(paddr - addr >= 15)
            {
                goto __input_error;
            }
            *(paddr++) = c;
            __col++;
            break;
        case ',':
        case '\r':
        case '\n':
        case ' ':
        case '\t':
            if(c == '\n')
            {
                __row++;
                __col = 1;
            } else
            {
                __col++;
            }
            if(paddr - addr > 0)
            {
                *paddr = '\0';
                rc = sscanf_s(addr, "%u.%u.%u.%u", &_a, &_b, &_c, &_d);
                if(rc != 4 || rc == EOF || _a > 255 || _b > 255 || _c > 255 || _d > 255)
                {
                    goto __invalid_ip_error;
                }
                if(!find_subnet(((_a << 24) | (_b << 16) | (_c << 8) | _d) & __mask))
                {
                    goto __memory_error;
                }
            }
            paddr = addr;
            break;
        default:
            if(feof(stdin))
            {
                break;
            }
            else
            {
                goto __input_error;
            }
            break;
        }
    }

    int __cidr_n;
    switch (__cidr)
    {
    case CIDR_24: __cidr_n = 24; break;
    case CIDR_16: __cidr_n = 16; break;
    case CIDR_8: __cidr_n = 8; break;
    }

    char __sepchars[3];
    switch(__sep)
    {
    case SEP_COMMA: __sepchars[0] = ','; __sepchars[1] = ' '; __sepchars[2] = 0; break;
    case SEP_LINE: __sepchars[0] = '\n'; __sepchars[1] = 0; break;
    }

    struct node *p = subnet_head;
    while(p)
    {
        switch(p->walk)
        {
        case WALK_LEFT:
            p->walk = WALK_RIGHT;
            if(p->left) p = p->left;
            break;
        case WALK_RIGHT:
            p->walk = WALK_END;
            if(p->right) p = p->right;
            break;
        case WALK_END:
            printf("%lu.%lu.%lu.%lu/%d", (p->addr >> 24), (p->addr >> 16 & 0xff), (p->addr >> 8 & 0xff), (p->addr & 0xff), __cidr_n);
            p = p->p;
            if(p)
            {
                fprintf(stdout, "%s", __sepchars);
            }
            break;
        }
    }

    free_nodes();

    return EXIT_SUCCESS;
__input_error:
    fprintf(stderr, "Invalid character at %d:%d\n", __row, __col);
    return EXIT_FAILURE;
__invalid_ip_error:
    fprintf(stderr, "Invalid ip addr: %s\n", addr);
    return EXIT_FAILURE;
__memory_error:
    fprintf(stderr, "Cannot allocate memory.\n");
    return EXIT_FAILURE;
__argument_error:
    fprintf(stderr, "Invalid argument.\n");
    print_help(stderr);
    return EXIT_FAILURE;
}