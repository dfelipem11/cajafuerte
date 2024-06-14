/**
 * @file
 * @brief Funciones para controlar un relé usando el contacto normalmente cerrado conectado a un pin GPIO de la 
 * RASPERRY PI PICO y una cerrudaru electrica.
 * 
 */

#ifndef CERRADURA_H
#define CERRADURA_H

#include "pico/stdlib.h"

/**
 * @brief Inicializa el pin GPIO del relé.
 * 
 * Configura el pin especificado como salida y lo establece en alto (3.3V) para desactivar el relé.
 */

void rele_init();

/**
 * @brief Enciende el relé.
 * 
 * Establece el pin del relé en bajo (0V) para activar el relé.
 */

void encender_rele();

/**
 * @brief Apaga el relé.
 * 
 * Establece el pin del relé en alto (3.3V) para desactivar el relé.
 */

void apagar_rele();


#endif // CERRADURA_H

