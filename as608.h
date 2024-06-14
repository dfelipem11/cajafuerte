/**
 * @file as608.h
 * @brief Declaraciones de funciones para el funcionamiento del lector AS608 con la Raspberry Pi Pico.
 */

#ifndef AS608_H
#define AS608_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Inicializa el sensor de huellas AS608.
 */
void as608_init(void);

/**
 * @brief Envía un comando al sensor de huellas AS608.
 * 
 * @param command Comando a enviar.
 * @param len Longitud del comando.
 */
void as608_send_command(const uint8_t *command, size_t len);

/**
 * @brief Lee una respuesta del sensor de huellas AS608 con un temporizador de espera.
 * 
 * @param response Buffer donde se almacenará la respuesta.
 * @param len Longitud esperada de la respuesta.
 * @return uint8_t Código de confirmación en la respuesta del sensor.
 */
uint8_t as608_read_response(uint8_t *response, size_t len);

/**
 * @brief Captura una imagen de huella dactilar del sensor AS608.
 * 
 * @return uint8_t Código de confirmación en la respuesta del sensor.
 */
uint8_t as608_get_image(void);

/**
 * @brief Convierte una imagen capturada en una plantilla y la almacena en el buffer especificado.
 * 
 * @param slot Buffer donde almacenar la plantilla (1 o 2).
 * @return uint8_t Código de confirmación en la respuesta del sensor.
 */
uint8_t as608_image_to_template(uint8_t slot);

/**
 * @brief Crea un modelo a partir de las dos plantillas almacenadas en los buffers.
 * 
 * @return uint8_t Código de confirmación en la respuesta del sensor.
 */
uint8_t as608_create_model(void);

/**
 * @brief Almacena una plantilla en la posición especificada.
 * 
 * @param id ID donde almacenar la plantilla.
 * @return uint8_t Código de confirmación en la respuesta del sensor.
 */
uint8_t as608_store_model(uint16_t id);

/**
 * @brief Busca una huella en la base de datos.
 * 
 * @return uint8_t Código de confirmación en la respuesta del sensor.
 */
uint8_t as608_search_model(void);

/**
 * @brief Elimina una huella de la base de datos.
 * 
 * @param id ID de la huella a eliminar.
 * @return uint8_t Código de confirmación en la respuesta del sensor.
 */
uint8_t as608_delete_model(uint16_t id);

/**
 * @brief Elimina todas las huellas dactilares de la base de datos.
 * 
 * @return uint8_t Código de confirmación en la respuesta del sensor.
 */
uint8_t as608_empty_database(void);

#endif // AS608_H

