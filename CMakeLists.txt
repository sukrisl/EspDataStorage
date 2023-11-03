cmake_minimum_required(VERSION 3.10)

idf_component_register(
    SRCS
        "EspDataStorage.cpp"
        "SPIFlash.cpp"
        "StorageDevice.cpp"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "."
    REQUIRES
        spi_flash
        esp_littlefs
)
