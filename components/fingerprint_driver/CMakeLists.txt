idf_component_register(
    SRCS "fingerprint_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer
)