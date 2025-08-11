set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

idf_component_register(SRCS "reflect.cpp"
                       PRIV_REQUIRES spi_flash nvs_flash esp_wifi
                       INCLUDE_DIRS ".")
