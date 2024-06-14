cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)

project(as608_fingerprint)
pico_sdk_init()

add_executable(as608_fingerprint
    main.c
    as608.c
    lcd_i2c_16x2.c
    cerradura.c
    as608.h
)

target_link_libraries(as608_fingerprint pico_stdlib hardware_uart hardware_spi hardware_i2c hardware_gpio hardware_pwm hardware_irq hardware_sync hardware_timer)

pico_enable_stdio_uart(as608_fingerprint 0)
pico_enable_stdio_usb(as608_fingerprint 1)

pico_add_extra_outputs(as608_fingerprint)
