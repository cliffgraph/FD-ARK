set(BinName "fd-ark")

add_executable(
	${BinName}
	title.cpp
	global.cpp
	fd-ark.cpp
	ctrlfd_core0_task.cpp
	ctrlfd_core1_task.cpp
	emu_core0_task.cpp
	emu_core1_task.cpp
	timerobj.cpp
	lcd.cpp
	extio.cpp
	sdfat.cpp
	ff/ff.c
	ff/ffsystem.c
	ff/ffunicode.c
	ff/mmc_pico_spi.c
)

include_directories(ff)

target_compile_options(
	${BinName}
	PUBLIC -O2 -Wall)

pico_generate_pio_header(
	${BinName}
	${CMAKE_CURRENT_LIST_DIR}/fd-ark.pio
)

## enable usb output, disable uart output
#pico_enable_stdio_usb(${BinName} 1)
#pico_enable_stdio_uart(${BinName} 0)

target_link_libraries(
	${BinName}
	PRIVATE
	pico_stdlib hardware_pio hardware_spi hardware_i2c pico_multicore)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(${BinName})

# add url via pico_set_program_url
#example_auto_set_url(${BinName})

