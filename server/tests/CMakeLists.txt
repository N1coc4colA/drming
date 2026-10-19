find_package(Qt6 REQUIRED COMPONENTS Test)

function(drming_add_test name)
    qt_add_executable(test_${name} ${ARGN})

    target_link_libraries(
    test_${name}
        PRIVATE
            drming_server_core
            Qt6::Test Qt6::Core Qt6::Gui Qt6::Network Qt6::DBus
            Libdrm::Libdrm
    )
    add_test(NAME ${name} COMMAND test_${name})

    # Quiet, parseable output when run from CTest
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_LOGGING_RULES=*.debug=false"
    )
endfunction()

drming_add_test(commandparser commandparser/tst_commandparser.cpp)
drming_add_test(packets       packets/tst_packets.cpp)
drming_add_test(jitterbuffer  packets/tst_jitterbuffer.cpp)
drming_add_test(drmformat     sse/tst_drmformat.cpp)
drming_add_test(socket        socket/tst_socket.cpp)
