cmake_minimum_required(VERSION 3.10)
project(TaskManager VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

# Generate GResource source file
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)
set(GRESOURCE_XML ${CMAKE_CURRENT_SOURCE_DIR}/resource/resources.xml)
set(GRESOURCE_C ${CMAKE_CURRENT_BINARY_DIR}/resources.c)

add_custom_command(
    OUTPUT ${GRESOURCE_C}
    COMMAND ${GLIB_COMPILE_RESOURCES} --generate-source --target=${GRESOURCE_C} ${GRESOURCE_XML}
    DEPENDS ${GRESOURCE_XML} ${CMAKE_CURRENT_SOURCE_DIR}/resource/IconApp.png
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resource
)

# Sources
set(SOURCES
    src/main.cpp
    src/ui.cpp
    src/process_manager.cpp
    src/service_manager.cpp
    ${GRESOURCE_C}
)

add_executable(taskmanager ${SOURCES})
target_include_directories(taskmanager PRIVATE include)
target_link_libraries(taskmanager PRIVATE ${GTK3_LIBRARIES})
target_include_directories(taskmanager PRIVATE ${GTK3_INCLUDE_DIRS})
target_compile_options(taskmanager PRIVATE ${GTK3_CFLAGS_OTHER})

# Installation Rules
include(GNUInstallDirs)

# Install executable
install(TARGETS taskmanager DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install wrapper script
install(PROGRAMS assets/run_taskmanager.sh DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME run_taskmanager)

# Install icon
install(FILES resource/IconApp.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps RENAME taskmanager.png)

# Install desktop file
install(FILES assets/taskmanager.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
