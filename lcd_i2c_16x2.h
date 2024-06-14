/**
 * @file
 * @brief Código de ejemplo para controlar un panel LCD 16x2 a través de un chip puente I2C (por ejemplo, PCF8574).
 *
 * Este código demuestra cómo controlar un panel LCD 16x2 utilizando un microcontrolador Raspberry Pi Pico y un chip puente I2C. El código inicializa el LCD, lo borra y proporciona funciones para establecer la posición del cursor, mostrar caracteres y borrar la pantalla. También incluye comandos para controlar la visualización y el cursor, como encenderlos o apagarlos.
 *
 * @note El panel debe ser capaz de ser manejado a 3.3v, no a 5v, ya que los GPIO del Pico (y por lo tanto el I2C) no pueden utilizarse a 5v. Si se ejecuta el panel a 5v, se necesitará un convertidor de nivel para las líneas I2C.
 *
 * Conexiones en la placa Raspberry Pi Pico, las conexiones en otras placas pueden variar:
 *
 * GPIO 0 (pin 1)-> SDA en la placa del puente LCD
 * GPIO 1 (pin 2)-> SCL en la placa del puente LCD
 * 3.3v (pin 36) -> VCC en la placa del puente LCD
 * GND (pin 38)  -> GND en la placa del puente LCD
 */


#ifndef lcd_i2c_16x2
#define lcd_i2c_16x2

/**
 * @brief Función para escribir un byte en el bus I2C.
 *
 * Esta función envía un byte a través del bus I2C utilizando el controlador
 * I2C predeterminado y la dirección del dispositivo especificada.
 *
 * @param val Valor del byte a escribir.
 */
void i2c_write_byte(uint8_t val);

/**
 * @brief Función para alternar el pin de habilitación en el display LCD.
 *
 * Esta función alterna el pin de habilitación en el display LCD para realizar
 * operaciones de lectura/escritura. Se incluyen retardos para asegurar el
 * funcionamiento correcto.
 *
 * @param val Valor del byte a enviar al display LCD.
 */
void lcd_toggle_enable(uint8_t val);

/**
 * @brief Función para enviar un byte al display LCD.
 *
 * Esta función envía un byte al display LCD utilizando el modo especificado
 * (comando o caracter) y controlando el backlight.
 *
 * @param val Valor del byte a enviar.
 * @param mode Modo de envío (LCD_COMMAND o LCD_CHARACTER).
 */
void lcd_send_byte(uint8_t val, int mode);

/**
 * @brief Función para limpiar la pantalla del display LCD.
 *
 * Esta función envía un comando para limpiar la pantalla del display LCD.
 */
void lcd_clear(void);

/**
 * @brief Función para establecer la posición del cursor en el display LCD.
 *
 * Esta función mueve el cursor del display LCD a la posición especificada.
 *
 * @param line Número de línea (0 o 1).
 * @param position Posición en la línea.
 */
void lcd_set_cursor(int line, int position);

/**
 * @brief Función para enviar un caracter al display LCD.
 *
 * Esta función envía un caracter al display LCD.
 *
 * @param val Caracter a enviar.
 */
static void inline lcd_char(char val);

/**
 * @brief Función para enviar una cadena de caracteres al display LCD.
 *
 * Esta función envía una cadena de caracteres al display LCD.
 *
 * @param s Cadena de caracteres a enviar.
 */
void lcd_string(const char *s);

/**
 * @brief Función para inicializar el display LCD.
 *
 * Esta función inicializa el display LCD con la configuración adecuada.
 * Se establece el modo de entrada, el número de líneas, el control del display
 * y se borra la pantalla.
 */
void lcd_init();

/**
 * @brief Función para inicializar variables y mostrar un mensaje en el display LCD.
 *
 * Esta función inicializa las variables necesarias para la comunicación I2C y
 * muestra un mensaje en el display LCD.
 *
 * @param message Mensaje a mostrar en el display LCD.
 * @param linea Indicador de línea (verdadero o falso).
 */
void initVar(char message[32], bool linea);

#endif
