/**
 * @file as608.c
 * @brief Funciones para el funcionamiento del lector AS608 con la raspberry pi pico
 */

#include "as608.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"



// Definiciones de UART
#define UART_ID uart1
#define BAUD_RATE 57600  // Asegúrate de usar la tasa de baudios correcta para el AS608

// Pines del UART
#define UART_TX_PIN 8
#define UART_RX_PIN 9

#define TIMEOUT_MS 10000  // Tiempo de espera máximo en milisegundos

volatile int index = 0;
/**
 * @brief Inicializa el sensor de huellas AS608.
 */
void as608_init() {
    stdio_init_all();
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    sleep_ms(5000);
}


/**
 * @brief Envía un comando al sensor de huellas AS608.
 * 
 * @param command Comando a enviar.
 * @param len Longitud del comando.
 */
void as608_send_command(const uint8_t *command, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uart_putc(UART_ID, command[i]);
    }
    // Enviar comando al módulo AS608
    printf("Enviando comando: ");
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", command[i]);
    }
    printf("\n");
}

/**
 * @brief Lee una respuesta del sensor de huellas AS608 con un temporizador de espera.
 * 
 * @param response Buffer donde se almacenará la respuesta.
 * @param len Longitud esperada de la respuesta.
 * @return true si la respuesta es válida, false en caso contrario.
 */
uint8_t as608_read_response(uint8_t *response, size_t len) {
    absolute_time_t timeout_time = make_timeout_time_ms(TIMEOUT_MS);
    index = 0;  // Reiniciar el índice de recepción
    
    while (index < len) {
        if (uart_is_readable(UART_ID)) {
            response[index] = uart_getc(UART_ID);
            printf("%02X ", response[index]);
            index=index+1;

        } else if (time_reached(timeout_time)) {
            printf("\n");
            index = 0;
            printf("Se demoro mas tiempo del que se esperaba.\n");
            return -1; // Salir si se supera el tiempo de espera
        }
        sleep_ms(10); // Esperar un poco entre caracteres
    }
    printf("\n");
    //print_response(*response, len);
    return response[9];
}


/**
 * @brief Verifica la contraseña del sensor AS608.
 * 
 * @return Código de estado del sensor.
 */
uint8_t as608_verify_password(void) {
    // Comando para verificar la contraseña
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x07, 0x13, 0x00, 0x00, 0x00, 0x00, 0x10, 0x1B};
    
    // Buffer para la respuesta del sensor
    uint8_t response[12];
    
    // Enviar el comando al sensor
    as608_send_command(cmd, sizeof(cmd));
    
    // Leer la respuesta del sensor
    uint8_t status = as608_read_response(response, 12);
    
    // Verificar el código de confirmación en la respuesta
    if (status == 0) {
        if (response[9] == 0x00) {
            printf("Contraseña verificada correctamente.\n");
        } else {
            printf("Error al verificar la contraseña: Código %02X\n", response[9]);
        }
    } else {
        printf("Error en la comunicación con el sensor.\n");
    }
    
    return response[9];
}

/**
 * @brief Captura una imagen de huella dactilar del sensor AS608.
 * 
 * @return Código de estado del sensor.
 */
uint8_t as608_get_image(void) {
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x01, 0x00, 0x05};
    uint8_t response[12];
    as608_send_command(cmd, sizeof(cmd));
    return as608_read_response(response, 12);
}

/**
 * @brief Convierte una imagen capturada en una plantilla y la almacena en el buffer especificado.
 * 
 * @param slot Buffer donde almacenar la plantilla (1 o 2).
 * @return Código de estado del sensor.
 */
uint8_t as608_image_to_template(uint8_t slot) {
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x04, 0x02, slot, 0x00, 0x07 + slot};
    uint8_t response[12];
    as608_send_command(cmd, sizeof(cmd));
    return as608_read_response(response, 12);
}

/**
 * @brief Crea un modelo a partir de las dos plantillas almacenadas en los buffers.
 * 
 * @return Código de estado del sensor.
 */
uint8_t as608_create_model(void) {
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x05, 0x00, 0x09};

    uint8_t response[12];
    as608_send_command(cmd, sizeof(cmd));
    return as608_read_response(response, 12);
}

/**
 * @brief Almacena una plantilla en la posición especificada.
 * 
 * @param id ID donde almacenar la plantilla.
 * @return Código de estado del sensor.
 */
uint8_t as608_store_model(uint16_t id) {
    uint8_t id_high = (id >> 8) & 0xFF; // Obtiene el byte alto del ID
    uint8_t id_low = id & 0xFF; // Obtiene el byte bajo del ID

    // Construye el comando para almacenar el modelo
    uint8_t cmd[] = {
        0xEF, 0x01, // Paquete de encabezado
        0xFF, 0xFF, 0xFF, 0xFF, // Dirección del dispositivo
        0x01, // Identificador del paquete
        0x00, 0x06, // Longitud del paquete
        0x06, // Instrucción (Store)
        0x01, // Buffer ID (indica el buffer de la imagen 2)
        id_high, // Byte alto del ID de la plantilla
        id_low, // Byte bajo del ID de la plantilla
        0x00, 0x00 // Checksum inicializado en 0 (se actualizará más adelante)
    };
    
    // Calcula el checksum sumando todos los bytes desde el identificador del paquete hasta el final del ID
    uint16_t checksum = 0x01 + 0x00 + 0x06 + 0x06 + 0x01 + id_high + id_low;
    cmd[13] = (checksum >> 8) & 0xFF; // Asigna el byte alto del checksum
    cmd[14] = checksum & 0xFF; // Asigna el byte bajo del checksum

    uint8_t response[12]; // Buffer para almacenar la respuesta del sensor
    as608_send_command(cmd, sizeof(cmd)); // Envía el comando al sensor
    return as608_read_response(response, 12); // Lee la respuesta del sensor y devuelve el código de estado
}


/**
 * @brief Busca una huella en la base de datos.
 * 
 * @return Código de estado del sensor.
 */
uint8_t as608_search_model(void) {
    uint8_t cmd[] = {
        0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x08, 0x04, 0x01, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00
    };
    uint16_t checksum = 0;
    for (int i = 6; i < 15; i++) {
        checksum += cmd[i];
    }
    cmd[15] = (checksum >> 8) & 0xFF;
    cmd[16] = checksum & 0xFF;
    
    uint8_t response[16];
    as608_send_command(cmd, sizeof(cmd));
    return as608_read_response(response, 16);
}


/**
 * @brief Elimina una huella de la base de datos.
 * 
 * @param id ID de la huella a eliminar.
 * @return Código de estado del sensor.
 */
uint8_t as608_delete_model(uint16_t id) {
    uint8_t id_high = (id >> 8) & 0xFF;
    uint8_t id_low = id & 0xFF;
    uint8_t cmd[] = {
        0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x07, // Longitud corregida
        0x0C, id_high, id_low, 0x00, 0x01, 0x00, 0x00
    };
    uint16_t checksum = 0;
    for (int i = 6; i < 14; i++) {
        checksum += cmd[i];
    }
    cmd[14] = (checksum >> 8) & 0xFF;
    cmd[15] = checksum & 0xFF;
    
    uint8_t response[12];
    as608_send_command(cmd, sizeof(cmd));
    return as608_read_response(response, 12);
}


/**
 * @brief Elimina todas las huellas dactilares de la base de datos.
 * 
 * @return Código de estado del sensor.
 */
uint8_t as608_empty_database(void) {
    // Comando para vaciar la base de datos
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x0D, 0x00, 0x11};
    
    // Buffer para la respuesta del sensor
    uint8_t response[12];
    
    // Enviar el comando al sensor
    as608_send_command(cmd, sizeof(cmd));
    
    // Leer la respuesta del sensor
    return as608_read_response(response, 12);
}


