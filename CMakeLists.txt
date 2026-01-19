cmake_minimum_required(VERSION 3.10)
project(8080Emu C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE EMU_SOURCES "src/*.c")
file(GLOB_RECURSE EMU_HEADERS "src/*.h")
file(GLOB_RECURSE GUI_SOURCES "gui/*.cpp")
file(GLOB_RECURSE GUI_HEADERS "gui/*.h")

include_directories(src GUI)

set(Qt6_DIR "C:/Qt/6.10.1/msvc2022_64/lib/cmake/Qt6")
find_package(Qt6 COMPONENTS Widgets REQUIRED)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(8080Emu
    ${EMU_SOURCES} ${EMU_HEADERS}
    ${GUI_SOURCES} ${GUI_HEADERS}
)

target_link_libraries(8080Emu PRIVATE Qt6::Widgets)