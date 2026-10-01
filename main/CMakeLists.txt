idf_component_register(SRCS "main.c" "screens.c" "sensor_task.c" "display_task.c" "sensor_processor.c" "ble_task.c" "alarm_task.c" "cpu_temp.c"
                       PRIV_REQUIRES spi_flash ip5306 esp_driver_gpio sh1107 imu_sensor nvs_flash bt esp_driver_ledc
                       INCLUDE_DIRS ".")
