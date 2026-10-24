cmake_minimum_required(VERSION 3.16)
project(drming_server VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network DBus)
find_package(Libdrm REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(AVAHI REQUIRED IMPORTED_TARGET avahi-client)
pkg_check_modules(
FFMPEG REQUIRED IMPORTED_TARGET
    libavformat libavcodec libavutil libswscale
)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../submodules/ctrack
    ${CMAKE_CURRENT_BINARY_DIR}/ctrack
)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../submodules/qt-aes
    ${CMAKE_CURRENT_BINARY_DIR}/qt-aes
)

# ---- Core library ------------------------------------------------------------
set(DRMING_SERVER_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/certificatesupport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/telemetry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/udp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../settings.h

    avahi_publisher.h avahi_publisher.cpp
    commandparser.h commandparser.cpp
    display.h display.cpp
    displaymanager.h displaymanager.cpp
    displayreader.h displayreader.cpp
    displaythread.h displaythread.cpp
    dispsetup.h dispsetup.cpp
    ffmpeg.h ffmpeg.cpp
    networkclient.h networkclient.cpp
    parameters.h parameters.cpp
    server.h server.cpp
    sse.h sse.cpp
    streamsocket.h
    utils.h utils.cpp
    vkmsfb.h
)

add_library(
drming_server_core
    STATIC
        ${DRMING_SERVER_CORE_SOURCES}
)

target_include_directories(
drming_server_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(
drming_server_core
    PUBLIC
        Qt6::Core Qt6::Gui Qt6::Network Qt6::DBus
        PkgConfig::AVAHI
        PkgConfig::FFMPEG
        QtAES::QtAES
        Libdrm::Libdrm
        ctrack
)

target_compile_definitions(
drming_server_core
    PRIVATE
        QT_NO_SIGNALS_SLOTS_KEYWORDS
        QT_NO_EMIT
)

# ---- Executable --------------------------------------------------------------
qt_add_executable(
drming_server
    MANUAL_FINALIZATION
        main.cpp
)
target_link_libraries(
drming_server
    PRIVATE
        drming_server_core
        ctrack
)

include(GNUInstallDirs)
install(TARGETS
drming_server
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

target_compile_definitions(
drming_server
    PRIVATE
        QT_NO_SIGNALS_SLOTS_KEYWORDS
        QT_NO_EMIT
)

qt_finalize_executable(drming_server)

# ---- Tests -------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_BINARY_DIR}/tests
    )
endif()
