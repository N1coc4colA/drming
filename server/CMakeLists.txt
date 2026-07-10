cmake_minimum_required(VERSION 3.16)

project(drming_server VERSION 0.1 LANGUAGES CXX)


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
    ../certificatesupport.h
    ../parser.h
    ../settings.h

    main.cpp

    avahi_publisher.h avahi_publisher.cpp
    commandparser.h commandparser.cpp
    display.h display.cpp
    displaymanager.h displaymanager.cpp
    displayreader.h displayreader.cpp
    dispsetup.h dispsetup.cpp
    networkclient.h networkclient.cpp
    parameters.h parameters.cpp
    server.h server.cpp
    sse.h sse.cpp
    utils.h utils.cpp
    vkmsfb.h
)


qt_add_executable(
drming_server
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}

)

target_link_libraries(
drming_server
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
TARGETS drming_server
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)


qt_finalize_executable(drming_server)
