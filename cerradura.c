/**
 * @file
 * @brief Funciones para controlar un relé usando el contacto normalmente cerrado conectado a un pin GPIO de la 
 * RASPERRY PI PICO y una cerrudaru electrica.
 * 
 */

#include "pico/stdlib.h"
#include "cerradura.h"


// Definir el pin GPIO donde está conectado el relé
#define RELE_PIN 19

// Inicializa el rele
void rele_init(){
    // Configurar el pin como salida
    gpio_init(RELE_PIN);
    gpio_set_dir(RELE_PIN, GPIO_OUT);
    gpio_put(RELE_PIN, 1);  // Establecer el pin en alto (3.3V)
}

// Enciende el rele
void encender_rele() {
    gpio_put(RELE_PIN, 0);  // Establecer el pin en bajo (0V)

}

// Apaga el rele
void apagar_rele() {
    gpio_put(RELE_PIN, 1);  // Establecer el pin en alto  (3.3V)
}

