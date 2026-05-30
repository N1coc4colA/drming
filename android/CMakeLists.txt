cmake_minimum_required(VERSION 3.16)

project(drming_android VERSION 0.1 LANGUAGES CXX)


set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g -O0")
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -g -O0")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network Widgets)


set(PROJECT_SOURCES
    ../certificatesupport.h
    ../parser.h

    main.cpp

    fileprovider.h fileprovider.cpp
    mdns.h mdns.cpp
    networklink.h networklink.cpp
    networkstatus.h networkstatus.cpp
    palette.h palette.cpp

    declarative/videoframeitem.h
    declarative/videoframeitem.cpp

    models/filesmodel.h
    models/filesmodel.cpp
    models/servicesmodel.h
    models/servicesmodel.cpp
)

if (ANDROID)
    file(GLOB_RECURSE PROJECT_PLATFORM_SOURCES ${PROJECT_SOURCE_DIR}/platform/android/*.cpp)
    file(GLOB_RECURSE PROJECT_PLATFORM_HEADERS ${PROJECT_SOURCE_DIR}/platform/android/*.h)
else()
    file(GLOB_RECURSE PROJECT_PLATFORM_SOURCES ${PROJECT_SOURCE_DIR}/platform/linux/*.cpp)
    file(GLOB_RECURSE PROJECT_PLATFORM_HEADERS ${PROJECT_SOURCE_DIR}/platform/linux/*.h)
endif()


qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(
drming_android
    ${PROJECT_SOURCES}
    ${PROJECT_PLATFORM_HEADERS}
    ${PROJECT_PLATFORM_SOURCES}
)

set_source_files_properties(
    qml/GlobalVars.qml

    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(drming_android
    URI drming
    QML_FILES
        qml/Main.qml
        qml/GlobalVars.qml

        qml/HomePage.qml
        qml/CertificatesPages.qml
        qml/KeysPage.qml
        qml/ServicesPage.qml
        qml/StreamPage.qml

        qml/SearchBar.qml
        qml/ServicesList.qml
        qml/ServiceSelector.qml
        qml/LoginDialog.qml

        qml/HeaderBar.qml
        qml/EasyButton.qml
        qml/EasyComboBox.qml
        qml/EasyDialog.qml
        qml/EasyListView.qml
        qml/EasyPage.qml
        qml/StandardPage.qml
        qml/NewButton.qml

        qml/TapAnimation.qml

    RESOURCES
        assets.qrc
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
        Qt6::QuickControls2
        Qt6::Widgets
)


if (NOT ANDROID)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(AVAHI REQUIRED avahi-client)

    target_link_libraries(drming_android PRIVATE ${AVAHI_LIBRARIES})
endif()
