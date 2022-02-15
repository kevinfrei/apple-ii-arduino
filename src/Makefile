# Some simple details
ifeq ($(OS),Windows_NT)
	ARD=${HOME}/AppData/Local
	SERIAL_PORT=COM8
	RUNTIME_HARDWARE_PATH=c:/PROGRA~2/Arduino/hardware/tools
	CMD_PATH=${RUNTIME_HARDWARE_PATH}
else ifeq ($(shell uname -s), Darwin)
	ARD=/Applications/Teensyduino.app/Contents/Java/hardware
	SERIAL_PORT=$(shell ls /dev/cu.usbmodem5*)
	TOOLS_PATH=${ARD}/tools
	RUNTIME_HARDWARE_PATH=${TOOLS_PATH}
	CMD_PATH=${TOOLS_PATH}
else
  $(error Linux support not tested yet)
endif
# Default: 600. Also okay: 24, 150, 396, 450, 528, 720, 816
# Require cooling: 912, 960, 1008 (anything above 600 gets kinda warm)
IN_SPEED=816
IN_USB=serialhid
IN_OPT=o3std
IN_KEYS=en-us
EXTRA_TIME_LOCAL=0
BOARD_NAME=teensy40
SERIAL_PORT_LABEL=${SERIAL_PORT}
SERIAL_PORT_PROTOCOL=serial

# Project Config stuff
PROJ_NAME=apple2
BUILD_PATH=out

# My custom flags
COMPILER_CPP_EXTRA_FLAGS=-flto

USER_INCLUDES=-Iinclude
USER_CPP_SRCS=\
	APPLEII.cpp \
	cassette.cpp \
	cpu.cpp \
	font.cpp \
	hooks.cpp \
	kbdemu.cpp \
	memory.cpp \
	screen.cpp \
	speaker.cpp

# Libraries to use:
LIB_BUSIO=1
LIB_SPI=1
LIB_GFX=1
LIB_ST77XX=1
LIB_WIRE=1

ifeq ($(OS),Windows_NT)
include teensy-win.inc
else
include teensy-mac.inc
endif

