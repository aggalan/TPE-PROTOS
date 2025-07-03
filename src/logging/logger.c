#include "logger.h"
#include <stdlib.h>
#include <unistd.h>

int debug_enabled = 0;

void logger_init() {
    debug_enabled = 0; // Deshabilitar depuracion por defecto
}
void logger_set_debug(int enabled) {
    debug_enabled = enabled;
}
int logger_is_debug_enabled() {
    return debug_enabled;
}
void logger_parse_args(int argc, char *argv[]) {
    int opt;
    
    
    optind = 1; // Reiniciar el índice de opciones para permitir múltiples llamadas a getopt
    
    while ((opt = getopt(argc, argv, "dh")) != -1) {
        switch (opt) {
            case 'd':
                logger_set_debug(1);
                #ifdef DEBUG
                LOG_INFO("Debug mode enabled...");
                #endif
                break;
            case 'h':
                printf("Uso: %s [-d] [-h]\n", argv[0]);
                printf("  -d: Use debug\n");
                printf("  -h: Help...duh\n");
                exit(0);
                break;
            case '?':
                fprintf(stderr, "Use -h for help\n");
                exit(1);
                break;
        }
    }
}
