cmake_minimum_required(VERSION 3.16)

# 指定交叉编译平台、工具链和语言标准。推荐使用 Ninja 生成器加快增量构建。
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_SIZE arm-none-eabi-size)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(basic_framework C ASM)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(MCU_FLAGS -mcpu=cortex-m4 -mthumb -mthumb-interwork -mfloat-abi=hard -mfpu=fpv4-sp-d16)
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/STM32F407IGHx_FLASH.ld")
set(CMSIS_DSP_LIB "${CMAKE_SOURCE_DIR}/Middlewares/ST/ARM/DSP/Lib/libCMSISDSP.a")
set(OPENOCD_FLASH_START "0x08000000" CACHE STRING "STM32 flash start address")

add_compile_options(
    -pipe
    ${MCU_FLAGS}
    -Wall
    -Werror
    -fmessage-length=0
    -ffunction-sections
    -fdata-sections
    -fno-common
)

add_link_options(
    -pipe
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,--no-warn-rwx-segments
    -Wl,--gc-sections
    -flto
    -specs=nano.specs
    -Wl,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map
    -Wl,--cref
    -Wl,--print-memory-usage
)

add_compile_definitions(
    USE_HAL_DRIVER
    STM32F407xx
    ARM_MATH_CM4
)

function(include_sub_directories_recursively root_dir)
    if(NOT IS_DIRECTORY "${root_dir}")
        return()
    endif()

    file(TO_CMAKE_PATH "${root_dir}" normalized_root)
    if(normalized_root MATCHES "/CMSIS_RTOS($|/)")
        return()
    endif()

    include_directories("${root_dir}")

    file(GLOB children LIST_DIRECTORIES true RELATIVE "${root_dir}" "${root_dir}/*")
    foreach(child ${children})
        if(IS_DIRECTORY "${root_dir}/${child}")
            include_sub_directories_recursively("${root_dir}/${child}")
        endif()
    endforeach()
endfunction()

include_sub_directories_recursively("${CMAKE_SOURCE_DIR}/Drivers")
include_sub_directories_recursively("${CMAKE_SOURCE_DIR}/Middlewares")
include_sub_directories_recursively("${CMAKE_SOURCE_DIR}/bsp")
include_sub_directories_recursively("${CMAKE_SOURCE_DIR}/modules")
include_sub_directories_recursively("${CMAKE_SOURCE_DIR}/application")
include_sub_directories_recursively("${CMAKE_SOURCE_DIR}/Inc")

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/Drivers/*.c"
    "${CMAKE_SOURCE_DIR}/Src/*.c"
    "${CMAKE_SOURCE_DIR}/Middlewares/*.c"
    "${CMAKE_SOURCE_DIR}/bsp/*.c"
    "${CMAKE_SOURCE_DIR}/modules/*.c"
    "${CMAKE_SOURCE_DIR}/application/*.c"
)

list(FILTER SOURCES EXCLUDE REGEX ".*/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/.*")
list(APPEND SOURCES "${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c")
list(REMOVE_DUPLICATES SOURCES)

set(ASM_SOURCES
    "${CMAKE_SOURCE_DIR}/startup_stm32f407xx.s"
    "${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/SEGGER/RTT/SEGGER_RTT_ASM_ARMv7M.s"
)
set_source_files_properties(${ASM_SOURCES} PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message(STATUS "Maximum optimization for speed")
    add_compile_options(-Ofast)
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    message(STATUS "Maximum optimization for speed, debug info included")
    add_compile_options(-Ofast -g)
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    message(STATUS "Maximum optimization for size")
    add_compile_options(-Os)
else()
    message(STATUS "Minimal optimization, debug info included")
    add_compile_options(-Og -g -gdwarf-2)
    add_compile_definitions(ESC_DEBUG)
endif()

add_executable(${PROJECT_NAME}.elf ${SOURCES} ${ASM_SOURCES} ${LINKER_SCRIPT})
target_link_libraries(${PROJECT_NAME}.elf PRIVATE ${CMSIS_DSP_LIB} m c)

add_custom_command(
    TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Building hex and bin files..."
)

find_program(OPENOCD_COMMAND openocd)
if(NOT OPENOCD_COMMAND)
    set(OPENOCD_COMMAND openocd)
endif()

add_custom_target(flash_dap
    COMMAND ${OPENOCD_COMMAND}
        -f "${CMAKE_SOURCE_DIR}/openocd_dap.cfg"
        -c init
        -c halt
        -c "flash write_image erase ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex ${OPENOCD_FLASH_START}"
        -c reset
        -c shutdown
    DEPENDS ${PROJECT_NAME}.elf
    USES_TERMINAL
    VERBATIM
    COMMENT "Flashing with OpenOCD CMSIS-DAP"
)

add_custom_target(flash_jlink
    COMMAND ${OPENOCD_COMMAND}
        -f "${CMAKE_SOURCE_DIR}/openocd_jlink.cfg"
        -c init
        -c halt
        -c "flash write_image erase ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex ${OPENOCD_FLASH_START}"
        -c reset
        -c shutdown
    DEPENDS ${PROJECT_NAME}.elf
    USES_TERMINAL
    VERBATIM
    COMMENT "Flashing with OpenOCD J-Link"
)
