# Let's try this, because Arduino is kind of a PITA
# I basically just copied the crap out of the build output and dumped it here
VER=0.8.1
ADAFRUIT=${HOME}/Library/Arduino15/packages/adafruit
TOOLS=${ADAFRUIT}/tools/gcc-arm-none-eabi/5_2-2015q4/
CC=${TOOLS}/bin/arm-none-eabi-gcc
CPP=${TOOLS}/bin/arm-none-eabi-g++
HWROOT=${ADAFRUIT}/hardware/nrf52/${VER}

DEFINES=-DF_CPU=64000000 -DARDUINO=10613 -DARDUINO_NRF52_FEATHER \
	-DARDUINO_ARCH_NRF52 -DARDUINO_BSP_VERSION="${VER}" \
	-DARDUINO_FEATHER52 -DARDUINO_NRF52_ADAFRUIT -DNRF5 -DNRF52 -DNRF52832_XXAA \
	-DUSE_LFXO -DS132 -DSD_VER=201 -DCFG_DEBUG=0
TARGET=-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CODEGEN=-nostdlib --param max-inline-insns-single=500 -ffunction-sections -fdata-sections -fno-threadsafe-statics -fno-rtti -fno-exceptions
FLAGS=-g -Wall -u _printf_float -MMD
LANG=-std=gnu++11 -w -x c++
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

CPPFLAGS=${TARGET} ${FLAGS} ${CODEGEN} ${LANG} ${DEFINES} ${OPT} ${INCLUDES}

all: left.o right.o
	@echo ${ADAFRUIT}

left.o: left-slave.cpp shared.h
	${CPP} -c ${CPPFLAGS} left-slave.cpp -o left.o

right.o: right-master.cpp keyhelpers.h keymap.h keystate.h shared.h
	${CPP} -c ${CPPFLAGS} right-master.cpp -o right.o

clean:
	-rm *.o

# DO NOT DELETE
