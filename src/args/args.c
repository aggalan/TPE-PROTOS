#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <errno.h>
#include <getopt.h>
#include "../admin/admin.h"

#include "args.h"
#include "logger.h"
struct socks5args socks5args;

static unsigned short
port(const char* s)
{
    char* end = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s || '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 0 || sl > USHRT_MAX)
    {
        fprintf(stderr, "port should in in the range of 1-65536: %s\n", s);
        exit(1);
    }
    return (unsigned short)sl;
}

static void
user(char* s, struct users* user,char * user_file)
{
    char* p = strchr(s, ':');
    if (p == NULL)
    {
        fprintf(stderr, "user not found\n");
        exit(1);
    }
    else
    {
        LOG_INFO("Adding user %s with password %s\n", s, p + 1);
        *p = 0;
        p++;
        user->name = s;
        user->pass = p;
        admin_add_user(user->name, user->pass, user_file);
        LOG_INFO("User %s added successfully.\n", user->name);
    }
}



static void
version(void)
{
    fprintf(stderr, "socks5v version 0.0\n"
            "ITBA Protocolos de Comunicación 2025/1 -- Grupo 6\n"
            "AQUI VA LA LICENCIA\n");
}

static void
usage(const char* progname)
{
    fprintf(stderr,
            "Usage: %s [OPTION]...\n"
            "\n"
            "   -h               Imprime la ayuda y termina.\n"
            "   -a <name>:<pass> Usuario y contraseña de usuario para los administradores de el proxy. Hasta 10.\n"
            "   -l <SOCKS addr>  Dirección donde servirá el proxy SOCKS.\n"
            "   -L <conf  addr>  Dirección donde servirá el servicio de management.\n"
            "   -p <SOCKS port>  Puerto entrante conexiones SOCKS.\n"
            "   -P <conf port>   Puerto entrante conexiones configuracion\n"
            "   -u <name>:<pass> Usuario y contraseña de usuario que puede usar el proxy. Hasta 10.\n"
            "   -v               Imprime información sobre la versión versión y termina.\n"

            "\n",
            progname);
    exit(1);
}

void
parse_args(const int argc, char** argv, struct socks5args* args)
{
    memset(args, 0, sizeof(*args));

    args->socks_addr = "0.0.0.0";
    args->socks_port = 1080;

    args->mng_addr = "127.0.0.1";
    args->mng_port = 8080;

    args->disectors_enabled = true;

    int c;
    int nusers = 0;
    int anusers = 0;

    while (true)
    {
        int option_index = 0;
        static struct option long_options[] = {
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "a:hl:L:Np:P:u:v", long_options, &option_index);        if (c == -1)
            break;

        switch (c)
        {case 'a':
            if (anusers >= MAX_ADMINS)
            {
                fprintf(stderr, "maximun number of command line users reached: %d.\n", MAX_ADMINS);
                exit(1);
            }
            else
            {
                user(optarg, args->admins + anusers,ADMIN_FILE);
                anusers++;
            }
            break;
            case 'h':
            usage(argv[0]);
            break;
        case 'l':
            args->socks_addr = optarg;
            break;
        case 'L':
            args->mng_addr = optarg;
            break;
        case 'N':
            args->disectors_enabled = false;
            break;
        case 'p':
            args->socks_port = port(optarg);
            break;
        case 'P':
            args->mng_port = port(optarg);
            break;
        case 'u':
            if (nusers >= MAX_USERS)
            {
                fprintf(stderr, "maximun number of command line users reached: %d.\n", MAX_USERS);
                exit(1);
            }
            else
            {
                user(optarg, args->users + nusers,USER_FILE);
                nusers++;
            }
            break;
        case 'v':
            version();
            exit(0);
        default:
            fprintf(stderr, "unknown argument %d.\n", c);
            exit(1);
        }
    }
    if (optind < argc)
    {
        fprintf(stderr, "argument not accepted: ");
        while (optind < argc)
        {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
        exit(1);
    }
}
