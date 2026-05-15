cmake_minimum_required(VERSION 3.16)

project(drming_desktop VERSION 0.1 LANGUAGES CXX)


set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)


find_package(Qt6 REQUIRED COMPONENTS Core Gui Network DBus)
find_package(Libdrm REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVAHI REQUIRED avahi-client)


set(PROJECT_SOURCES
    main.cpp
    dispsetup.h dispsetup.cpp
    displayreader.h displayreader.cpp
    vkmsfb.h
    avahi_publisher.h avahi_publisher.cpp
    commandparser.h commandparser.cpp
)


qt_add_executable(
drming_desktop
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    server.h server.cpp

)

target_link_libraries(
drming_desktop
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::DBus
        ${AVAHI_LIBRARIES}
        Libdrm::Libdrm
)


include(GNUInstallDirs)
install(
TARGETS drming_desktop
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)


qt_finalize_executable(drming_desktop)
