# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/maya/esp/v5.3.3/esp-idf/components/bootloader/subproject"
  "/home/maya/prog/C/embedded/esp32_blink/build/bootloader"
  "/home/maya/prog/C/embedded/esp32_blink/build/bootloader-prefix"
  "/home/maya/prog/C/embedded/esp32_blink/build/bootloader-prefix/tmp"
  "/home/maya/prog/C/embedded/esp32_blink/build/bootloader-prefix/src/bootloader-stamp"
  "/home/maya/prog/C/embedded/esp32_blink/build/bootloader-prefix/src"
  "/home/maya/prog/C/embedded/esp32_blink/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/maya/prog/C/embedded/esp32_blink/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/maya/prog/C/embedded/esp32_blink/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
