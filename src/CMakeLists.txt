add_executable(${ProjectName}
        main.cpp
        critical_section.cpp
        Fmutex.cpp
        Fmutex.h

        ipstack/IPStack.cpp
        ipstack/IPStack.h
        ipstack/lwipopts.h
        ipstack/tls_common.c
        ipstack/picow_tls_client.c
        ipstack/lwipopts_tls.h

        i2c/PicoI2C.cpp
        i2c/PicoI2C.h

        uart/PicoOsUart.cpp
        uart/PicoOsUart.h

        modbus/nanomodbus.h
        modbus/nanomodbus.c
        modbus/ModbusRegister.cpp
        modbus/ModbusRegister.h
        modbus/ModbusClient.cpp
        modbus/ModbusClient.h

        display/framebuf.cpp
        display/framebuf.h
        display/mono_vlsb.cpp
        display/mono_vlsb.h
#        display/ssd1306.cpp
#        display/ssd1306.h
        display/ssd1306os.h
        display/ssd1306os.cpp

        controller/controller_task.cpp
        controller/controller_task.h

        drivers/bme68x.c
        drivers/bme68x.h
        drivers/bme68x_defs.h
        drivers/stepper.h
        drivers/stepper.cpp
        drivers/GPIO_input.cpp
        drivers/GPIO_input.h
        drivers/pump.h
        drivers/pump.cpp
        drivers/ultrasonic_sensor.cpp
        drivers/ultrasonic_sensor.h
        drivers/EEPROM.cpp
        drivers/EEPROM.h

        tasks/bme_task.cpp
        tasks/bme_task.h
        tasks/oled_task.h
        tasks/oled_task.cpp
        tasks/stepper_task.h
        tasks/stepper_task.cpp
        tasks/gpio_task.cpp
        tasks/gpio_task.h
        tasks/ultrasonic_task.h
        tasks/ultrasonic_task.cpp
        tasks/pump_task.h
        tasks/pump_task.cpp
        tasks/lid_ultrasonic_task.cpp
        tasks/lid_ultrasonic_task.h
        tasks/EEPROM_task.cpp
        tasks/EEPROM_task.h
        tasks/mqtt_task.cpp
        tasks/mqtt_task.h

        mqtt/MQTTPacket/MQTTConnectClient.c
        mqtt/MQTTPacket/MQTTDeserializePublish.c
        mqtt/MQTTPacket/MQTTFormat.c
        mqtt/MQTTPacket/MQTTPacket.c
        mqtt/MQTTPacket/MQTTSerializePublish.c
        mqtt/MQTTPacket/MQTTSubscribeClient.c
        mqtt/MQTTPacket/MQTTUnsubscribeClient.c

)

target_include_directories(${ProjectName} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
        modbus
        uart
        i2c
        display
        ipstack
        mqtt
        mqtt/MQTTPacket
)

target_compile_definitions(${ProjectName} PRIVATE
        WIFI_SSID=\"$ENV{WIFI_SSID}\"
        WIFI_PASSWORD=\"$ENV{WIFI_PASSWORD}\"
        NO_SYS=0            # don't want NO_SYS (generally this would be in your lwipopts.h)
        PICO_CYW43_ARCH_DEFAULT_COUNTRY_CODE=CYW43_COUNTRY_FINLAND
)

target_link_libraries(${ProjectName} 
        pico_stdlib
        hardware_i2c
        FreeRTOS-Kernel-Heap4
        pico_cyw43_arch_lwip_sys_freertos
        pico_lwip_mbedtls
        pico_mbedtls
        hardware_pwm

)

# Ignore warnings from lwip code
set_source_files_properties(
        ${PICO_LWIP_PATH}/src/apps/altcp_tls/altcp_tls_mbedtls.c
        PROPERTIES
        COMPILE_OPTIONS "-Wno-unused-result"
)


# Disable usb output, enable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)