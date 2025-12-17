idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        fingerprint_driver
        mp3_driver
        display_driver
        keypad_driver
        network_manager
        time_manager
        system_tasks
        nvs_flash
        esp_wifi
        esp_netif
)