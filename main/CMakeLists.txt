idf_component_register(SRCS "main.c" "screens.c" "sensor_task.c" "display_task.c" "sensor_processor.c" "ble_task.c" "alarm_task.c"
                       PRIV_REQUIRES spi_flash ip5306 driver sh1107 mpu6886 nvs_flash bt esp_driver_ledc
                       INCLUDE_DIRS ".")
