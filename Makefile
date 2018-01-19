# Let's try this, because Arduino is kind of a PITA
# I basically just copied the crap out of the build output and dumped it here

# Some simple details
OUT=out

# AdaFruit library installation location/version malarkey
VER=0.8.1
ADAFRUIT=${HOME}/Library/Arduino15/packages/adafruit
TOOLS=${ADAFRUIT}/tools/gcc-arm-none-eabi/5_2-2015q4
HWROOT=${ADAFRUIT}/hardware/nrf52/${VER}

# Tools (probably don't need to change these at all)
CC=${TOOLS}/bin/arm-none-eabi-gcc
CPP=${TOOLS}/bin/arm-none-eabi-g++
OBJCOPY=${TOOLS}bin/arm-none-eabi-objcopy

# Flags for compilation
DEFINES=-DF_CPU=64000000 -DARDUINO=10613 -DARDUINO_NRF52_FEATHER \
-DARDUINO_ARCH_NRF52 -DARDUINO_BSP_VERSION="${VER}" \
-DARDUINO_FEATHER52 -DARDUINO_NRF52_ADAFRUIT -DNRF5 -DNRF52 -DNRF52832_XXAA \
-DUSE_LFXO -DS132 -DSD_VER=201 -DCFG_DEBUG=0
TARGET=-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CODEGEN=-nostdlib --param max-inline-insns-single=500 -ffunction-sections -fdata-sections
FLAGS=-g -Wall -u _printf_float -MMD
CPPLANG=-std=gnu++11 -w -x c++ -fno-rtti -fno-exceptions -fno-threadsafe-statics
CLANG=-std=gnu11 -DSOFTDEVICE_PRESENT
OPT=-Os

INCLUDES=\
	"-I${HWROOT}/cores/nRF5/SDK/components/toolchain/"\
	"-I${HWROOT}/cores/nRF5/SDK/components/toolchain/cmsis/include"\
	"-I${HWROOT}/cores/nRF5/SDK/components/toolchain/gcc/"\
	"-I${HWROOT}/cores/nRF5/SDK/components/device/"\
	"-I${HWROOT}/cores/nRF5/SDK/components/drivers_nrf/delay/"\
	"-I${HWROOT}/cores/nRF5/SDK/components/drivers_nrf/hal/"\
	"-I${HWROOT}/cores/nRF5/SDK/components/libraries/util/"\
	"-I${HWROOT}/cores/nRF5/softdevice/s132/2.0.1/headers/"\
	"-I${HWROOT}/cores/nRF5/freertos/source/include"\
	"-I${HWROOT}/cores/nRF5/freertos/config"\
	"-I${HWROOT}/cores/nRF5/freertos/portable/GCC/nrf52"\
	"-I${HWROOT}/cores/nRF5/freertos/portable/CMSIS/nrf52"\
	"-I${HWROOT}/cores/nRF5/sysview/SEGGER"\
	"-I${HWROOT}/cores/nRF5/sysview/Config"\
	"-I${HWROOT}/libraries/nffs/src/fs/nffs/include"\
	"-I${HWROOT}/libraries/nffs/src/fs/fs/include"\
	"-I${HWROOT}/libraries/nffs/src/fs/disk/include"\
	"-I${HWROOT}/libraries/nffs/src/util/crc/include"\
	"-I${HWROOT}/libraries/nffs/src/kernel/os/include"\
	"-I${HWROOT}/libraries/nffs/src/kernel/os/include/os/arch/cortex_m4"\
	"-I${HWROOT}/libraries/nffs/src/hw/hal/include"\
	"-I${HWROOT}/libraries/nffs/src/sys/flash_map/include"\
	"-I${HWROOT}/libraries/nffs/src/sys/defs/include"\
	"-I${HWROOT}/cores/nRF5"\
	"-I${HWROOT}/variants/feather52"\
	"-I${HWROOT}/libraries/Bluefruit52Lib/src"\
	"-I${HWROOT}/libraries/nffs/src"

CPPFLAGS=${TARGET} ${FLAGS} ${CODEGEN} ${CPPLANG} ${DEFINES} ${OPT} ${INCLUDES}
CFLAGS=${TARGET} ${FLAGS} ${CODEGEN} ${CLANG} ${DEFINES} ${OPT} ${INCLUDES}


# Actual rules for building

all: ${OUT} ${OUT}/left.o ${OUT}/right.o
	@echo ${ADAFRUIT}

${OUT}:
	-mkdir ${OUT}

${OUT}/left.o: left-slave.cpp shared.h
	${CPP} -c ${CPPFLAGS} left-slave.cpp -o ${OUT}/left.o

${OUT}/right.o: right-master.cpp keyhelpers.h keymap.h keystate.h shared.h
	${CPP} -c ${CPPFLAGS} right-master.cpp -o ${OUT}/right.o

clean:
	-rm -rf ${OUT}

${OUT}/right.hex: ${OUT}/right.elf
	${OBJCOPY) -O ihex

#"${TOOLS}/bin/arm-none-eabi-objcopy" -O ihex  "${OUTPUT}/RightHand_master.ino.elf" "${OUTPUT}/RightHand_master.ino.hex"
#/usr/local/bin/nrfutil --verbose dfu serial -pkg ${OUTPUT}/RightHand_master.ino.zip -p /dev/cu.SLAB_USBtoUART -b 115200
