cmake_minimum_required(VERSION 3.16)
project(jazz_recomp C)

# Jazz Jackrabbit (1994): FILE0001.EXE, a Borland Pascal protected-mode NE
# program under RTM, lifted by pcrecomp's ne_link (tools/relift.py) and run on
# its Win16 host, with the DOS machine in runtime/.

set(CMAKE_C_STANDARD 11)

set(PCRECOMP_DIR "${CMAKE_SOURCE_DIR}/../pcrecomp" CACHE PATH "pcrecomp checkout")
set(WIN16_RT "${PCRECOMP_DIR}/runtime/win16")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_C_FLAGS MATCHES "-O")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g1")
endif()

set(WORK_DIR "${CMAKE_SOURCE_DIR}/work" CACHE PATH "generated tree")
file(GLOB SEGMENT_SOURCES CONFIGURE_DEPENDS "${WORK_DIR}/src/*.c")
if(NOT SEGMENT_SOURCES)
    message(FATAL_ERROR "No lifted sources in ${WORK_DIR}/src. Run: python tools/relift.py")
endif()

include_directories("${WORK_DIR}/runtime" "${WIN16_RT}" "${WIN16_RT}/host" "${PCRECOMP_DIR}/runtime")
# A DOS program on the Win16 CPU: real port I/O, the VGA behind A000, and the
# timer and keyboard interrupts delivered from loop back-edges.
add_compile_definitions(RECOMP_PORT_IO RECOMP_MEM_HOOK RECOMP_IRQ)

add_library(jazz_segments STATIC ${SEGMENT_SOURCES})
target_compile_options(jazz_segments PRIVATE -Wall -Wno-unused-variable -Wno-unused-label
    -Wno-unused-but-set-variable -Wno-unused-function)

file(GLOB RUNTIME_SOURCES CONFIGURE_DEPENDS "${WIN16_RT}/host/*.c")
list(FILTER RUNTIME_SOURCES EXCLUDE REGEX "/main\.c$")
add_library(jazz_runtime STATIC ${RUNTIME_SOURCES} runtime/jazz.c runtime/vga.c)
target_compile_definitions(jazz_runtime PRIVATE
    RECOMP_GAME_DIR="${CMAKE_SOURCE_DIR}/game"
    RECOMP_DATA_DIR="${CMAKE_SOURCE_DIR}/game")

add_executable(jazz "${WIN16_RT}/host/main.c")
target_compile_definitions(jazz PRIVATE RECOMP_IMAGE_PATH="${WORK_DIR}/mem_image.bin")
target_link_libraries(jazz jazz_segments jazz_runtime jazz_segments m user32 gdi32 winmm)
target_link_options(jazz PRIVATE -Wl,--stack,268435456 -Wl,--disable-dynamicbase
                                 -Wl,--disable-high-entropy-va)
