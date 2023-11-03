# EspDataStorage
~~~
Warning: Very early code!
~~~

A C++ library component for ESP-IDF to managing app data using littlefs file system. This component supports the usage of external SPI flash that is connected to VSPI line of ESP32.

## Hardware Configuration
The external SPI flash needs to be connected to VSPI `(SPI_3)` line on ESP32, an attempt to connect the flash to another SPI line may result in failure.
```
[Flash]        [ESP32]
 DO ----------- GPIO19 (VSPI_MISO)
 DI ----------- GPIO23 (VSPI_MOSI)
 CLK ---------- GPIO18 (VSPI_SCLK)
 CS ----------- GPIO5  (VSPI_CS)
```
## Adding component to your ESP-IDF project
To use the library you can manually clone the repo or add component as a submodule. Go to your project directory on the terminal and add repo as a git submodule:
```
git submodule add https://github.com/sukrisl/EspDataStorage.git components/EspDataStorage
```