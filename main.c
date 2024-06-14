/**
 * @file main.c
 * @brief Sistema de caja fuerte con doble autenticación (teclado matricial y lector de huellas AS608) 
 * y visualización en LCD 16x2.
 */


#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "as608.h"
#include "lcd_i2c_16x2.h"
#include "cerradura.h"
// Definiciones de UART
#define UART_ID uart1
#define BAUD_RATE 57600  // Asegúrate de usar la tasa de baudios correcta para el AS608

// Pines del UART
#define UART_TX_PIN 8
#define UART_RX_PIN 9

char mensaje[32] = "                                ";

volatile bool Inicio=true;
volatile bool opciones=true;
volatile bool EtapaLector=false;
volatile uint8_t tarea=0;
volatile uint8_t rep=0;
volatile uint8_t mala=1;
volatile uint8_t UbicacionLector=0;

typedef union {
    uint8_t W;
    struct {
        bool keyFlag : 1; ///< Bandera para indicar que se ha presionado una tecla
        bool keyDbnc : 1; ///< Bandera para el debounce del teclado
        uint8_t : 6;      ///< Relleno no utilizado
    } B;
} myFlags_t;

volatile myFlags_t gFlags; ///< Flags globales para control de eventos

uint8_t vecPSWD[] = {
    0x4, 0x3, 0x2, 0x1,   // User 1 con contraseña 1234
    0x1, 0x2, 0x3, 0x4,   // User 2 con contraseña 4321
    0x0, 0x0, 0x0, 0x0,   // User 3 con contraseña 0000
    0x1, 0x1, 0x1, 0x1,   // User 4 con contraseña 1111
    0x2, 0x2, 0x2, 0x2,   // User 5 con contraseña 2222
    0x3, 0x3, 0x3, 0x3,   // User 6 con contraseña 3333
    0x4, 0x4, 0x4, 0x4,   // User 7 con contraseña 4444
    0x5, 0x5, 0x5, 0x5,   // User 8 con contraseña 5555
    0x6, 0x6, 0x6, 0x6    // User 9 con contraseña 6666
};

uint8_t vecIDs[4] = {0x0A, 0x0B, 0x0C, 0x0D}; ///< Vectores de comandos permitidos

uint8_t hKeys[1] = {0xFF}; ///< Historial de teclas ingresadas en el teclado

uint8_t IDlector[10] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09}; ///< IDs válidos para el lector

uint8_t InPasswords[4] = {0xFF, 0xFF, 0xFF, 0xFF}; ///< Contraseña ingresada por el usuario

volatile uint8_t key_cnt = 0; ///< Contador de veces que se ha presionado un teclado

volatile uint8_t gSeqCnt = 0; ///< Contador de secuencia
volatile bool gDZero = false; ///< Debouncer del teclado

volatile bool PasswordAcept = false; ///< Bandera para indicar aceptación de contraseña

volatile uint32_t gKeyCap; ///< Captura de la tecla presionada

/*
   Comportamiento del teclado
*/

/**
 * @brief Inserta una tecla en el historial.
 * 
 * @param key Tecla a insertar
 */
void insertKey(uint8_t key) {
    hKeys[0] = key;
    printf("Key[0]: %x\n", hKeys[0]);
}

/**
 * @brief Inserta una tecla en el historial de contraseñas ingresadas por el usuario.
 * 
 * @param key Tecla a insertar como parte de la contraseña
 */
void insertPswd(uint8_t key) {
    for (int i = 2; i >= 0; i--) {
        InPasswords[i + 1] = InPasswords[i];
    }
    InPasswords[0] = key;
    printf("Key[0]: %x\n", InPasswords[0]);
    printf("Key[1]: %x\n", InPasswords[1]);
    printf("Key[2]: %x\n", InPasswords[2]);
    printf("Key[3]: %x\n", InPasswords[3]);
}

/**
 * @brief Decodifica la tecla presionada.
 * 
 * @param keyc Código de la tecla
 * @return uint8_t Tecla decodificada
 */
uint8_t keyDecode(uint32_t keyc) {
    uint8_t keyd = 0xFF;
    switch (keyc) {
        case 0x88:
            keyd = 0x01;
            break;
        case 0x48:
            keyd = 0x02;
            break;
        case 0x28:
            keyd = 0x03;
            break;
        case 0x18:
            keyd = 0x0A;
            break;
        case 0x84:
            keyd = 0x04;
            break;
        case 0x44:
            keyd = 0x05;
            break;
        case 0x24:
            keyd = 0x06;
            break;
        case 0x14:
            keyd = 0x0B;
            break;
        case 0x82:
            keyd = 0x07;
            break;
        case 0x42:
            keyd = 0x08;
            break;
        case 0x22:
            keyd = 0x09;
            break;
        case 0x12:
            keyd = 0x0C;
            break;
        case 0x81:
            keyd = 0x0E;
            break;
        case 0x41:
            keyd = 0x00;
            break;
        case 0x21:
            keyd = 0x0F;
            break;
        case 0x11:
            keyd = 0x0D;
            break;
    }
    return keyd;
}

/**
 * @brief Verifica el ID ingresado con los IDs válidos.
 * 
 * @param vecID Vector de IDs válidos
 * @param ID ID ingresado para verificar
 * @return int8_t Índice del ID válido encontrado, o -1 si no se encontró.
 */
int8_t checkID(uint8_t *vecID, uint8_t *ID) {
    for (int i = 0; i < 4; i++) {
        printf("ID Valido[%d]: %x\n", i, vecID[i]);
        printf("Tecla ingresada: %x\n", ID[0]);

        if (vecID[i] == ID[0]) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Verifica el ID del lector de huella ingresado con los IDs válidos del lector.
 * 
 * @param IDlector Vector de IDs válidos del lector
 * @param ID ID del lector ingresado para verificar
 * @return int8_t Índice del ID válido encontrado, o -1 si no se encontró.
 */
int8_t checkIDlector(uint8_t *IDlector, uint8_t *ID) {
    for (int i = 0; i < 8; i++) {
        printf("ID Valido[%d]: %x\n", i, IDlector[i]);
        printf("Tecla ingresada: %x\n", ID[0]);

        if (IDlector[i] == ID[0]) {
            return i + 1;
        }
    }
    return -1;
}

/**
 * @brief Verifica la contraseña ingresada con las contraseñas almacenadas.
 * 
 * @param vecPSWD Vector de contraseñas almacenadas
 * @param PSWD Contraseña ingresada para verificar
 * @param i Posición de la contraseña a usar
 * @return int8_t Índice de la contraseña válida encontrada, o -1 si no se encontró.
 */
int8_t checkPSW2(uint8_t *vecPSWD, uint8_t *PSWD, uint8_t i) {
    bool flag = true;
    for (int j = 0; j < 4; j++) {
        printf("%X,%X\n", vecPSWD[4 * i + j], PSWD[j]);

        if (vecPSWD[4 * i + j] != PSWD[j]) {
            flag = false;
            break;
        }
    }
    if (flag == true){
        return i;
    }
    else{
        return -1;
    }       
    
}

/**
 * @brief Callback del teclado matricial
 * 
 * @param num Número del GPIO
 * @param mask Máscara del GPIO
 */
void keyboardCallback(uint8_t num, uint32_t mask) {
    gKeyCap = gpio_get_all();
    gFlags.B.keyFlag = true;
    pwm_set_enabled(0, false);   ///< Congela la secuencia de filas
    pwm_set_enabled(1, true);    ///< Activa la secuencia de columnas
    gpio_set_irq_enabled(14, GPIO_IRQ_EDGE_RISE, false);  
    gpio_set_irq_enabled(15, GPIO_IRQ_EDGE_RISE, false);  
    gpio_set_irq_enabled(16, GPIO_IRQ_EDGE_RISE, false);  
    gpio_set_irq_enabled(17, GPIO_IRQ_EDGE_RISE, false);
    gpio_acknowledge_irq(num, mask);   ///< Reconoce la interrupción del GPIO
}

/**
 * @brief Manejador de interrupciones de PWM
 */
void pwmIRQ(void) {
    switch (pwm_get_irq_status_mask()) {
        case 0x01UL:    ///< PWM slice 0 ISR utilizado como PIT para generar secuencia de filas
            gSeqCnt = (gSeqCnt + 1) % 4;
            gpio_put_masked(0x00003C00, 0x00000001 << (gSeqCnt + 10));
            pwm_clear_irq(0);   ///< Reconoce la interrupción del PWM en la rebanada 0
            break;
        case 0x02UL:    ///< PWM slice 1 ISR utilizado como PIT para implementar el debounce
            gFlags.B.keyDbnc = true;
            pwm_clear_irq(1);   ///< Reconoce la interrupción del PWM en la rebanada 1
            break;
        default:
            printf("Ocurrió algo inesperado en la IRQ de PWM\n");
            break;
    }
}

/**
 * @brief Inicializa el PWM como un PIT
 * 
 * @param slice Rebanada de PWM a utilizar
 * @param milis Tiempo en milisegundos
 * @param enable Habilita el PWM
 */
void initPWMasPIT(uint8_t slice, uint16_t milis, bool enable) {
    assert(milis <= 262);   ///< El PWM puede manejar períodos de interrupción mayores a 262 milisegundos
    float prescaler = (float)SYS_CLK_KHZ / 500;
    assert(prescaler < 256); ///< La parte entera del divisor de reloj puede ser mayor que 255 
    uint32_t wrap = 500000 * milis / 2000;
    assert(wrap < ((1UL << 17) - 1));
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_phase_correct(&cfg, true);
    pwm_config_set_clkdiv(&cfg, prescaler);
    pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_FREE_RUNNING);
    pwm_config_set_wrap(&cfg, wrap);
    pwm_set_irq_enabled(slice, true);
    irq_set_enabled(PWM_IRQ_WRAP, true);
    pwm_init(slice, &cfg, enable);
}

/**
 * @brief Inicializa el teclado matricial 4x4 utilizando GPIOs 2 a 9
 */
void initMatrixKeyboard4x4(void) {
    /// GPIOs 13 a 10 controlan las filas del teclado (secuencia one hot)
    /// GPIOS 17 a 14 controlan las columnas del teclado (IRQs GPIO)
    /// Configuramos quién controla el PAD GPIO
    gpio_set_function(10, GPIO_FUNC_SIO);
    gpio_set_function(11, GPIO_FUNC_SIO);
    gpio_set_function(12, GPIO_FUNC_SIO);
    gpio_set_function(13, GPIO_FUNC_SIO);
    gpio_set_function(14, GPIO_FUNC_SIO);
    gpio_set_function(15, GPIO_FUNC_SIO);
    gpio_set_function(16, GPIO_FUNC_SIO);
    gpio_set_function(17, GPIO_FUNC_SIO);

    gpio_set_dir_in_masked(0x0003C000);     ///< Configura gpios 14 a 17 como entradas (columnas)
    gpio_set_dir_out_masked(0x00003C00);    ///< Configura gpios 10 a 13 como salidas (filas)
    gpio_put_masked(0x00003C00, 0);         ///< Escribe 0 en las filas

    gpio_set_irq_enabled_with_callback(14, GPIO_IRQ_EDGE_RISE, true, keyboardCallback);
    gpio_set_irq_enabled_with_callback(15, GPIO_IRQ_EDGE_RISE, true, keyboardCallback);
    gpio_set_irq_enabled_with_callback(16, GPIO_IRQ_EDGE_RISE, true, keyboardCallback);
    gpio_set_irq_enabled_with_callback(17, GPIO_IRQ_EDGE_RISE, true, keyboardCallback);
}

/**
 * @brief Programa principal.
 * En el ciclo principal se desarrolla toda la implementación de la Caja Fuerte +.
 * Aqui se utilizan las librerias elaboradas para el lector de huella AS608 y para el LCD 16x2 que funciona
 * por I2C. La implementación de las funciones del teclado matricial se hizo por lógica
 * de interrupciones y se hizo en este mismo archivo.
 */
int main() {
    as608_init();
    rele_init();
    //sleep_ms(1000);
    printf("COMIENZOOOOOOOOOOOOO");
    printf("\n");
    // Comando de sincronización y comando de prueba para el módulo AS608
    uint8_t sync_byte = 0xAA;
    uint8_t RXbuye[1];
    // Enviar comando de sincronización al módulo AS608
    uart_putc(UART_ID, sync_byte);
    // Enviar comando al módulo AS608
    printf("Enviando comando: ");
    for (size_t i = 0; i < sizeof(sync_byte); i++) {
        printf("%02X ", sync_byte);
    }
    printf("\n");
    if (uart_is_readable(UART_ID)) {
        RXbuye[0] = uart_getc(UART_ID);
    }
    printf("Respuesta recibida: ");
    printf("%02X ", RXbuye[0]);
    printf("\n");
    //lcd_clear();
    strcpy(mensaje, "A:Reg B:Ing   C:Borr D:Vac");
    initVar(mensaje,true);
    initPWMasPIT(0,2,true);
    initPWMasPIT(1,100,false);
    irq_set_exclusive_handler(PWM_IRQ_WRAP,pwmIRQ);
    irq_set_priority(PWM_IRQ_WRAP, 0xC0);
    initMatrixKeyboard4x4();
    // Inicia el Bucle infinito de funcionamiento de la Caja fuerte
    while(1){
        // Funcionamiento del teclado matricial
        while(gFlags.W && !EtapaLector){
            if(gFlags.B.keyFlag){
                // Parte donde se escribe la password del ID seleccionado por el Usuario
                if(!opciones && !PasswordAcept && tarea==2 ){
               
                    uint32_t KeyData = (gKeyCap>>10) & 0x000000FF;
                    uint8_t keyd = keyDecode(KeyData);
                    printf("%X,%X\n",KeyData,keyd);
                    if(keyd!=0xFF){
                        insertPswd(keyd);
                    }
                    key_cnt++;
                    if(key_cnt==4){
                        // Implementa la función diseñada y compara la contraseña ingresada con la de
                        // la base de datos.
                        int8_t idxPW = checkPSW2(vecPSWD,&InPasswords[0],UbicacionLector-1);
                        printf("Key[0]: %x\n",InPasswords[0]);
                        printf("Key[1]: %x\n",InPasswords[1]);
                        printf("Key[2]: %x\n",InPasswords[2]);
                        printf("Key[3]: %x\n",InPasswords[3]);
                        if(idxPW==-1){
                            printf("Contrasena incorrecta %x\n");
                            strcpy(mensaje, "ERROR: Intente de Nuevo");
                            initVar(mensaje,true);
                        }
                        else {
                        
                            printf("Acceso consedido: %x\n");
                            strcpy(mensaje, "CONTRASENA CORRECTA  ");
                            initVar(mensaje,true);
                            sleep_ms(2500);
                            PasswordAcept = true;
                            EtapaLector=true;
                        }
                        key_cnt=0; 
                        }
                }
                // Aqui es la parte donde se escribe el ID en el que se desea registrar, ingresar o borrar
                if(opciones && !Inicio ){
                
                    uint32_t KeyData = (gKeyCap>>10) & 0x000000FF;
                    uint8_t keyd = keyDecode(KeyData);
                    printf("%X,%X\n",KeyData,keyd);
                    if(keyd!=0xFF){
                        insertKey(keyd);
                    }
                    key_cnt++;
                    if(key_cnt==1){
                        int8_t idxID = checkIDlector(IDlector,&hKeys[0]);
                        if (idxID == 1 || idxID == 2 || idxID == 3 || idxID == 4 || idxID == 5 || idxID == 6 || idxID == 7 || idxID == 8 || idxID == 9){
                            strcpy(mensaje, ("Seleccionaste Usuario # : %x\n",hKeys[0]));
                            initVar(mensaje,true);
                            printf("Seleccionaste Huella : %x\n",hKeys[0]);
                            sleep_ms(2500);
                            opciones=false;
                            UbicacionLector=idxID;  
                            if(tarea==2){
                                printf("Escribe la contraseña\n");
                                strcpy(mensaje, "ESCRIBA SU    CONTRASENA");
                                initVar(mensaje,true);
                            }
                            else{
                                EtapaLector=true;
                                printf("VAS AL LECTOR\n");
                                strcpy(mensaje, "PASAS A LECTURA DE HUELLA");
                                initVar(mensaje,true);
                                sleep_ms(2500);
                            }
                        }
                        else{
                            printf("No Seleccionas huella valida, vuelve a hacerlo\n"); 
                        }
                        key_cnt=0;
                        }
                }
                // Primera parte del código, donde seleccionada en el teclado el modo de operación
                if(Inicio ){
                
                    uint32_t KeyData = (gKeyCap>>10) & 0x000000FF;
                    uint8_t keyd = keyDecode(KeyData);
                    printf("%X,%X\n",KeyData,keyd);
                    if(keyd!=0xFF){
                        insertKey(keyd);
                    }
                    key_cnt++;
                    if(key_cnt==1){
                        int8_t idxID = checkID(vecIDs,&hKeys[0]);

                        if(idxID==0){
                            printf("Presionaste A, nueva huella agregar %x\n",hKeys[0]);
                            Inicio=false;
                            tarea=1;
                            //lcd_clear();
                            strcpy(mensaje, "Reg: ID huella 1-9");
                            initVar(mensaje,true);
                            printf("Selecciona una Huella 1-9");
                        }
                        else if(idxID==1){
                            printf("Oprimiste B Escribe la contraseña %x\n",hKeys[0]);
                            Inicio=false;
                            //opciones=false;
                            tarea=2;
                            strcpy(mensaje, "Ing: Indique     # Usuario ");
                            initVar(mensaje,true);
                            
                            
                        }
                        else if(idxID==2){
                            printf("Oprimiste C BORRA UNA HUELLA %x\n",hKeys[0]);
                            Inicio=false; 
                            tarea=3;
                            strcpy(mensaje, "Borr: Indique Borrar 1-9");
                            initVar(mensaje,true);
                            printf("Selecciona una Huella 1-9");
                        }
                        else if(idxID==3){
                            printf("Oprimiste D, BORRADO BASE DE DATOS %x\n",hKeys[0]);
                            Inicio=false;
                            opciones=false;
                            tarea=4;
                            EtapaLector=true;
                            strcpy(mensaje, "Vac: Vaciar Base de datos");
                            initVar(mensaje,true);
                            
                        }
                        else {
                        
                            printf("No presionaste una tecla valida\n"); 
                            strcpy(mensaje, "ERROR: TECLA INVALIDA REPEAT");
                            initVar(mensaje,true);
                            sleep_ms(2000);
                            strcpy(mensaje, "A:Reg B:Ing   C:Borr D:Vac");
                            initVar(mensaje,true);
                        }
                        key_cnt=0;
                    }
                
                }
            
            
                gFlags.B.keyFlag = false;
            }
            // Antirrebote de las interrupciones
            if(gFlags.B.keyDbnc){
                uint32_t keyc = gpio_get_all() & 0x0003C000; ///< Get raw gpio values
                if(gDZero){                                 
                    if(!keyc){               
                        pwm_set_enabled(0,true);                                           ///< Froze the row sequence
                        pwm_set_enabled(1,false);
                        gpio_set_irq_enabled(14,GPIO_IRQ_EDGE_RISE,true);  
                        gpio_set_irq_enabled(15,GPIO_IRQ_EDGE_RISE,true);  
                        gpio_set_irq_enabled(16,GPIO_IRQ_EDGE_RISE,true);  
                        gpio_set_irq_enabled(17,GPIO_IRQ_EDGE_RISE,true);
                    }
                    gDZero = false;
                }
                else{
                    gDZero = true;
                }
                gFlags.B.keyDbnc = false;
            }
        
        }
        // Luego de terminada la parte del teclado y contraseñas se pasa al modulo de lectura
        // de huella
        while (EtapaLector){
            printf("Entrooooooooooooooooooooo\n");
            // Parte donde se registra una nueva huella en la memoria del lector
            if (tarea==1){
                strcpy(mensaje, "Ponga la huella de su dedo.");
                initVar(mensaje,true);
                // Esperar un poco antes de enviar el siguiente comando
                sleep_ms(2500);
                // Se establece un limite de 3 intentos para registro de huella, sino no la guarda

                while(rep!= 3){      
                    printf("Capturando imagen\n");
                    
                    if (as608_get_image() == 0) {
                        printf("Imagen capturada\n");

                        printf("Convirtiendo imagen a plantilla\n");
                        if (as608_image_to_template(1) == 0) {
                            printf("Imagen convertida a plantilla\n");
                            
                            printf("Retire y vuelva a poner la huella de nuevo\n");
                            strcpy(mensaje, "Retire y vuelvala a poner.");
                            initVar(mensaje,true);
                            sleep_ms(2500);
                            if (as608_get_image() == 0) {
                                printf("Imagen capturada\n");

                                printf("Convirtiendo imagen a plantilla\n");
                                if (as608_image_to_template(2) == 0) {
                                    printf("Imagen convertida a plantilla\n");

                                    printf("Creando modelo...\n");
                                    if (as608_create_model() == 0) {
                                        printf("Modelo creado.\n");

                                        printf("Almacenando modelo...\n");
                                        if (as608_store_model(UbicacionLector) == 0) {
                                            printf("Modelo almacenado, ya puede retirar la huella.\n");
                                            strcpy(mensaje, "Huella Guardada. Quite el dedo.");
                                            initVar(mensaje,true);
                                            sleep_ms(4000);
                                            rep=3;
                                            mala=0;
                                        } else {
                                            printf("Error al almacenar el modelo.\n");
                                            strcpy(mensaje, "Error. Retire y vuelva a ponerla.");
                                            initVar(mensaje,true);
                                            sleep_ms(2500);
                                            rep++;

                                        }
                                    } else {
                                        printf("Error al crear el modelo.\n");
                                        printf("Retire y vuelva a poner la huella de nuevo\n");
                                        strcpy(mensaje, "Error. Retire y vuelva a ponerla.");
                                        initVar(mensaje,true);
                                        sleep_ms(2500);
                                        rep++;
                                    }
                                } else {
                                    printf("Error al convertir la imagen a plantilla (Intento 2).\n");
                                    printf("Retire y vuelva a poner la huella de nuevo\n");
                                    strcpy(mensaje, "Error. Retire y vuelva a ponerla.");
                                    initVar(mensaje,true);
                                    sleep_ms(2500);
                                    rep++;
                                }
                            } else {
                                printf("Error al capturar la imagen (Intento 2).\n");
                                printf("Retire y vuelva a poner la huella de nuevo\n");
                                strcpy(mensaje, "Error. Retire y vuelva a ponerla.");
                                initVar(mensaje,true);
                                sleep_ms(2500);
                                rep++;
                            }
                        } else {
                            printf("Error al convertir la imagen a plantilla (Intento 1).\n");
                            printf("Retire y vuelva a poner la huella de nuevo\n");
                            strcpy(mensaje, "Error. Retire y vuelva a ponerla.");
                            initVar(mensaje,true);
                            sleep_ms(2500);
                            rep++;
                        }
                    } else {
                        printf("Error al capturar la imagen (Intento 1).\n");
                        printf("Retire y vuelva a poner la huella de nuevo\n");
                        strcpy(mensaje, "Error. Retire y vuelva a ponerla.");
                        initVar(mensaje,true);
                        sleep_ms(2500);
                        rep++;
                    }

                }
                if (mala==1){
                    strcpy(mensaje, "ALcanzaste max intentos. Bloqueo.");
                    initVar(mensaje,true);
                }

            
            }
            // Aqui se hace la lectura de las huellas en la base de datos para dar acceso
            // Si esta en la base de datos, abre la caja fuerte
            if (tarea==2){
                strcpy(mensaje, "Ponga la huella de su dedo.");
                initVar(mensaje,true);
                // Esperar un poco antes de enviar el siguiente comando
                sleep_ms(2500);
                while(rep!= 3){    
                    printf("Capturando imagen...\n");
                    if (as608_get_image() == 0) {
                        printf("Imagen capturada.\n");

                        printf("Convirtiendo imagen a plantilla...\n");
                        if (as608_image_to_template(1) == 0) {
                            printf("Imagen convertida a plantilla.\n");

                            printf("Buscando modelo...\n");
                            if (as608_search_model() == 0) {
                                printf("Modelo encontrado.\n");
                                encender_rele();
                                // Aqui se implementa función de apertura de caja fuerte
                                strcpy(mensaje, "Acceso            Concedido");
                                initVar(mensaje,true);
                                sleep_ms(4000);
                                apagar_rele();
                                mala=0;
                                rep=3;
                            } else {
                                printf("Error al buscar el modelo.\n");
                                printf("Retire y vuelva a poner la huella de nuevo\n");
                                strcpy(mensaje, "Huella Incorrecta, Vuelva e intente.");
                                initVar(mensaje,true);

                                sleep_ms(2500);
                                rep++;
                            }
                        } else {
                            printf("Error al convertir la imagen a plantilla.\n");
                            printf("Retire y vuelva a poner la huella de nuevo\n");
                            strcpy(mensaje, "Error. Retire y vuelva a ponerla.");
                            initVar(mensaje,true);
                            sleep_ms(2500);
                            rep++;
                        }
                    } else {
                        printf("Error al capturar la imagen.\n");
                        printf("Retire y vuelva a poner la huella de nuevo\n");
                        strcpy(mensaje, "Error. Retire y vuelva a ponerla.");
                        initVar(mensaje,true);
                        sleep_ms(2500);
                        rep++;
                    }
                }
                if (mala==1){
                    strcpy(mensaje, "ALcanzaste max intentos. Bloqueo.");
                    initVar(mensaje,true);
                }
            }
            //Función que borra de la memoria una huella en especifico
            if (tarea==3){
                sleep_ms(2500);
                printf("Eliminando modelo...\n");
                if (as608_delete_model(UbicacionLector) == 0) {
                    printf("Modelo eliminado.\n");
                    strcpy(mensaje, "Modelo           eliminado.");
                    initVar(mensaje,true);
                    sleep_ms(4000);
                } else {
                    printf("Error al eliminar el modelo.\n");
                    strcpy(mensaje, "Error al eliminar    el modelo.");
                    initVar(mensaje,true);
                }
            }
            // función que borra toda la base de datos
            if (tarea==4){
                sleep_ms(2500);
                printf("Vaciando base de datos...\n");
                if (as608_empty_database() == 0) {
                    printf("Base de datos vaciada.\n");
                    strcpy(mensaje, "Base de datos      vaciada.");
                    initVar(mensaje,true);
                    sleep_ms(4000);
                    
                } else {
                    printf("Error al vaciar la base de datos.\n");
                    strcpy(mensaje, "Error al vaciar base de datos.");
                    initVar(mensaje,true);

                }
            }
            // Se reinician todas las banderas para que el programa vuelva a ejecutarse ciclicamente
            Inicio=true;
            opciones=true;
            EtapaLector=false;
            printf("as608_get_image: %s\n", EtapaLector ? "true" : "false");
            tarea=0;
            rep=0;
            mala=1;
            UbicacionLector=0;
            printf("LISTO PARA VOLVER A EMPEZAR\n");
            strcpy(mensaje, "CAJA FUERTE DISPONIBLE");
            initVar(mensaje,true);
            sleep_ms(2000);
            strcpy(mensaje, "A:Reg B:Ing   C:Borr D:Vac");


            
        }
        //__wfi();
        //printf("AFUERAAAAAAAAAAAAAAAA\n");
    }
    
}
