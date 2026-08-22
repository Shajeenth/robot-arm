idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    EMBED_FILES "index.html"
    REQUIRES esp_http_server esp_wifi nvs_flash esp_driver_ledc
)