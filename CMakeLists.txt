cmake_minimum_required(VERSION 3.10)
project(Nabdhu VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# Generate GResource source file
# (Removed to fix icon loading crash on root)

# Sources
set(SOURCES
    src/main.cpp
    src/ui.cpp
    src/process_manager.cpp
    src/service_manager.cpp
    src/network_manager.cpp
    src/disk_manager.cpp
)

add_executable(nabdhu ${SOURCES})
target_include_directories(nabdhu PRIVATE include)
target_link_libraries(nabdhu PRIVATE ftxui::screen ftxui::dom ftxui::component)

# Installation Rules
include(GNUInstallDirs)

# Install executable
install(TARGETS nabdhu DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install wrapper script
install(PROGRAMS assets/run_nabdhu.sh DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME run_nabdhu)

# Install icon
install(FILES resource/NabdhuRedIco.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps RENAME nabdhu.png OPTIONAL)

# Install desktop file
install(FILES assets/nabdhu.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
