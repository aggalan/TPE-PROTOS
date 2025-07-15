#ifndef NEGOTIATION_PARSER_H
#define NEGOTIATION_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../buffer/buffer.h"

/**
 * @file negotiation_parser.h
 * @brief Parser para la fase de negociación del protocolo SOCKS5
 * 
 * Este módulo implementa un parser de estado finito para procesar
 * la negociación inicial SOCKS5 donde el cliente propone métodos
 * de autenticación y el servidor selecciona uno.
 */

// Forward declaration del tipo buffer
// typedef struct buffer buffer;

/**
 * @brief Estados del parser de negociación
 */
typedef enum {
    NEG_VERSION,    /**< Esperando byte de versión SOCKS */
    NEG_NMETHODS,   /**< Esperando número de métodos de autenticación */
    NEG_METHODS,    /**< Leyendo métodos de autenticación */
    NEG_END,        /**< Negociación completada exitosamente */
    NEG_ERROR       /**< Error en la negociación */
} NegState;

/**
 * @brief Métodos de autenticación SOCKS5
 */
typedef enum {
    NO_AUTH = 0x00,     /**< Sin autenticación requerida */
    USER_PASS = 0x02,   /**< Autenticación usuario/contraseña */
    NO_METHOD = 0xFF    /**< No hay métodos aceptables */
} AuthMethod;

/**
 * @brief Códigos de resultado de la negociación
 */
typedef enum {
    NEG_OK,              /**< Operación exitosa */
    NEG_FULL_BUFFER,     /**< Buffer lleno, no se puede escribir */
} NegCodes;

/**
 * @brief Estructura del parser de negociación
 */
typedef struct {
    NegState state;         /**< Estado actual del parser */
    AuthMethod auth_method; /**< Método de autenticación seleccionado */
    uint8_t version;        /**< Versión del protocolo SOCKS */
    uint8_t nmethods;       /**< Número de métodos restantes por procesar */
} NegParser;

// ============================================================================
// API PÚBLICA
// ============================================================================

/**
 * @brief Inicializa el parser de negociación
 * 
 * Configura el parser en su estado inicial, listo para procesar
 * una nueva negociación SOCKS5. Establece todos los campos en
 * sus valores por defecto.
 * 
 * @param parser Puntero al parser a inicializar
 * 
 * @note Si parser es NULL, la función retorna sin hacer nada
 * @note Establece el estado inicial en NEG_VERSION
 * @note Inicializa auth_method en NO_METHOD
 */
void init_negotiation_parser(NegParser *parser);

/**
 * @brief Procesa un byte de datos de negociación
 * 
 * Función principal del parser que procesa byte por byte los datos
 * recibidos del cliente durante la fase de negociación. Actualiza
 * el estado interno del parser según el protocolo SOCKS5.
 * 
 * @param parser Puntero al parser de negociación
 * @param byte Byte a procesar
 * @return NegState Nuevo estado del parser después de procesar el byte
 * 
 * @note La función utiliza una tabla de funciones para manejar cada estado
 * @note Actualiza automáticamente el estado del parser
 */
NegState negotiation_parse(NegParser *parser, buffer * buffer);

/**
 * @brief Verifica si la negociación ha terminado exitosamente
 * 
 * @param parser Puntero al parser de negociación
 * @return bool true si la negociación terminó exitosamente, false en caso contrario
 * 
 * @note Retorna false si parser es NULL
 * @note Una negociación terminada significa que se seleccionó un método válido
 */
bool has_negotiation_read_ended(NegParser *parser);

/**
 * @brief Verifica si ocurrieron errores durante la negociación
 * 
 * @param parser Puntero al parser de negociación
 * @return bool true si hay errores, false en caso contrario
 * 
 * @note Retorna false si parser es NULL
 * @note Los errores incluyen versiones inválidas o estados inconsistentes
 */
bool has_negotiation_errors(NegParser *parser);

/**
 * @brief Llena el buffer con la respuesta de negociación
 * 
 * Genera la respuesta del servidor para la negociación SOCKS5,
 * indicando al cliente qué método de autenticación se utilizará.
 * La respuesta sigue el formato: [VERSIÓN][MÉTODO_SELECCIONADO].
 * 
 * @param parser Puntero al parser con el método seleccionado
 * @param buffer Puntero al buffer donde escribir la respuesta
 * @return NegCodes Resultado de la operación:
 *         - NEG_OK: Respuesta generada exitosamente
 *         - NEG_FULL_BUFFER: No hay espacio en el buffer
 *         - NEG_INVALID_METHOD: Método seleccionado es inválido
 * 
 * @note La respuesta tiene exactamente 2 bytes de longitud
 * @note Si auth_method es NO_METHOD, retorna NEG_INVALID_METHOD
 */
NegCodes fill_negotiation_answer(NegParser *parser, buffer *buffer);
#endif /* NEGOTIATION_PARSER_H */