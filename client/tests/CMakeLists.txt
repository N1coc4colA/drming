find_package(Qt6 REQUIRED COMPONENTS Test)

function(drming_add_test name)
    qt_add_executable(test_${name} ${ARGN})

    target_link_libraries(
    test_${name}
        PRIVATE
            drming_client_core
            Qt6::Test Qt6::Core Qt6::Gui Qt6::Network Qt6::DBus Qt6::Quick Qt6::QuickControls2 Qt6::Widgets QtAES::QtAES
            PkgConfig::FFMPEG
    )
    add_test(NAME ${name} COMMAND test_${name})

    if (NOT ANDROID)
        target_link_libraries(test_${name} PRIVATE
            ${AVAHI_LIBRARIES}
        )
    endif()

    # Quiet, parseable output when run from CTest
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_LOGGING_RULES=*.debug=false"
    )
endfunction()

drming_add_test(socket socket/tst_socket.cpp)
