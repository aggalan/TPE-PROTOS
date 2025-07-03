#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>
#include <string.h>

// Definir colores para la salida (opcional)
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

// Niveles de log
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR
} log_level_t;

// Variables globales
extern int debug_enabled;

// Función para obtener timestamp
static inline void get_timestamp(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Función para obtener solo el nombre del archivo (sin path completo)
static inline const char* get_filename(const char* path) {
    const char* filename = strrchr(path, '/');
    return filename ? filename + 1 : path;
}

// Macros principales de logging
#ifdef DEBUG
    #define LOG_DEBUG(fmt, ...) \
        do { \
            if (debug_enabled) { \
                char timestamp[64]; \
                get_timestamp(timestamp, sizeof(timestamp)); \
                printf(COLOR_CYAN "[DEBUG]" COLOR_RESET " [%s] %s:%d in %s(): " fmt "\n", \
                       timestamp, get_filename(__FILE__), __LINE__, __func__, ##__VA_ARGS__); \
                fflush(stdout); \
            } \
        } while(0)
#else
    #define LOG_DEBUG(fmt, ...) do {} while(0)
#endif

#define LOG_INFO(fmt, ...) \
    do { \
        char timestamp[64]; \
        get_timestamp(timestamp, sizeof(timestamp)); \
        printf(COLOR_GREEN "[INFO]" COLOR_RESET " [%s] " fmt "\n", \
               timestamp, ##__VA_ARGS__); \
        fflush(stdout); \
    } while(0)

#define LOG_WARNING(fmt, ...) \
    do { \
        char timestamp[64]; \
        get_timestamp(timestamp, sizeof(timestamp)); \
        printf(COLOR_YELLOW "[WARNING]" COLOR_RESET " [%s] %s:%d: " fmt "\n", \
               timestamp, get_filename(__FILE__), __LINE__, ##__VA_ARGS__); \
        fflush(stdout); \
    } while(0)

#define LOG_ERROR(fmt, ...) \
    do { \
        char timestamp[64]; \
        get_timestamp(timestamp, sizeof(timestamp)); \
        fprintf(stderr, COLOR_RED "[ERROR]" COLOR_RESET " [%s] %s:%d in %s(): " fmt "\n", \
                timestamp, get_filename(__FILE__), __LINE__, __func__, ##__VA_ARGS__); \
        fflush(stderr); \
    } while(0)

// Funciones de utilidad
void logger_init(void);
void logger_set_debug(int enabled);
int logger_is_debug_enabled(void);
void logger_parse_args(int argc, char* argv[]);

#endif // LOGGER_H