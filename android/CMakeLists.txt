cmake_minimum_required(VERSION 3.16)

project(drming_android VERSION 0.1 LANGUAGES CXX)


set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g -O0")
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -g -O0")


find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Network)


set(PROJECT_SOURCES
    main.cpp
    mdnsmanager.h mdnsmanager.cpp
    native.h native.cpp
    networklink.h networklink.cpp
    networkstatus.h networkstatus.cpp
    servicesmodel.h servicesmodel.cpp
    videoframeitem.h videoframeitem.cpp
)


qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(
drming_android
    ${PROJECT_SOURCES}
)

qt_add_qml_module(drming_android
    URI drming
    QML_FILES
        qml/Main.qml
        qml/LoginDialog.qml
        qml/SearchBar.qml
        qml/ServicesList.qml
        qml/ServiceSelector.qml
)

set_target_properties(
drming_android
    PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
        QT_ANDROID_VERSION_CODE 1
        QT_ANDROID_VERSION_NAME "1.0"
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 34
)

target_link_libraries(
drming_android
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Quick
)


if(ANDROID)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(AVAHI REQUIRED avahi-client)

    target_link_libraries(drming_android PRIVATE ${AVAHI_LIBRARIES})
endif()
