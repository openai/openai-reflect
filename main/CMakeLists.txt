set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

idf_component_register(SRCS ${SOURCES}
                       PRIV_REQUIRES spi_flash nvs_flash esp_wifi esp_http_server
                       INCLUDE_DIRS ".")

idf_component_get_property(lib sepfy__srtp COMPONENT_LIB)
target_compile_options(${lib} PRIVATE -Wno-error=incompatible-pointer-types)
