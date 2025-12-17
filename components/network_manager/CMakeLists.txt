idf_component_register(
    SRCS "network_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif esp_http_client nvs_flash esp_event
)