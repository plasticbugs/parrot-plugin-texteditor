# Text Editor (TEXTEDIT.CVP): a plain text editor, typed on a USB keyboard,
# that opens and saves .txt files on the card. The editor itself is the
# SDK's (sdk/firmware/revc/sdk/cvedit.h). This is the plugin around it.
#
#   cmake -B build -G Ninja -DPICO_SDK_PATH=$HOME/pico-sdk
#   ninja -C build                      -> build/TEXTEDIT.CVP
#
# Copy TEXTEDIT.CVP into the card's /PLUGINS folder.
cmake_minimum_required(VERSION 3.13)

# the cart's SDK: the copy in sdk/ (sdk/README.md)
set(PARROTCART_SDK ${CMAKE_CURRENT_LIST_DIR}/sdk/firmware/revc)
set(PICO_BOARD_HEADER_DIRS ${PARROTCART_SDK}/cvcart_revc/boards)
set(PICO_BOARD cvcart_revc)
set(PICO_PLATFORM rp2350-arm-s)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
# the USB host: sdk/tinyusb (TinyUSB 0.21 and its fixes since), not the pico-sdk's 0.18
include(${PARROTCART_SDK}/sdk/cvpad_tinyusb.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/pico_sdk_import.cmake)

project(textedit C CXX ASM)
pico_sdk_init()
include(${PARROTCART_SDK}/sdk/cvplugin.cmake)

# The same source makes the same file, whenever it's built: the compiler's
# __DATE__ and __TIME__ (the build date in the binary info) come from
# SOURCE_DATE_EPOCH, fixed here at the tested build's day, 2026-11-17.
set(BUILD_EPOCH 1794916800)
set(CMAKE_C_COMPILER_LAUNCHER ${CMAKE_COMMAND} -E env SOURCE_DATE_EPOCH=${BUILD_EPOCH})
set(CMAKE_CXX_COMPILER_LAUNCHER ${CMAKE_COMMAND} -E env SOURCE_DATE_EPOCH=${BUILD_EPOCH})

add_executable(textedit src/textedit.c)
target_compile_options(textedit PRIVATE -Wall -Wextra -Wno-stringop-truncation)
cvkernel_use(textedit)
target_include_directories(textedit PRIVATE ${PARROTCART_SDK}/midihello)
cvcart_features(textedit KEYBOARD CARD)
cvedit_use(textedit)
pico_enable_stdio_usb(textedit 0)
pico_enable_stdio_uart(textedit 0)
cvcart_plugin(textedit ID parrotcart.texteditor NAME "Text Editor" VERSION 0.1.0 BY "Parrot Cart"
              HOME /TEXT OPENS .txt ICON src/textedit_icon.txt FILE_ICON src/textedit_file_icon.txt
              FILE TEXTEDIT.CVP README CART-README.md)
