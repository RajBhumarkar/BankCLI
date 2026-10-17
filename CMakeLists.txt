cmake_minimum_required(VERSION 3.16)
project(BankCLI LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Pass -DMYSQL_CONNECTOR_ROOT="path/to/extracted/connector".
set(MYSQL_CONNECTOR_ROOT "" CACHE PATH "MySQL Connector/C++ installation")
find_path(MYSQL_INCLUDE mysql_driver.h
    HINTS "${MYSQL_CONNECTOR_ROOT}/include/jdbc" "${MYSQL_CONNECTOR_ROOT}/include")
find_library(MYSQL_LIBRARY NAMES mysqlcppconn
    HINTS "${MYSQL_CONNECTOR_ROOT}/lib64/vs14" "${MYSQL_CONNECTOR_ROOT}/lib64"
          "${MYSQL_CONNECTOR_ROOT}/lib")
if(NOT MYSQL_INCLUDE OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "Connector/C++ JDBC headers/library not found. Set MYSQL_CONNECTOR_ROOT; see README.md.")
endif()

add_executable(BankCLI main.cpp Account.cpp Database.cpp BankService.cpp)
target_include_directories(BankCLI SYSTEM PRIVATE "${MYSQL_INCLUDE}")
target_link_libraries(BankCLI PRIVATE "${MYSQL_LIBRARY}")
if(MSVC)
    target_compile_options(BankCLI PRIVATE /W4 /permissive-)
    target_compile_definitions(BankCLI PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(BankCLI PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The Windows runtime also needs the connector and its OpenSSL DLLs.
if(WIN32)
    file(GLOB MYSQL_DLLS "${MYSQL_CONNECTOR_ROOT}/lib64/*.dll"
                        "${MYSQL_CONNECTOR_ROOT}/bin/*.dll")
    foreach(DLL IN LISTS MYSQL_DLLS)
        add_custom_command(TARGET BankCLI POST_BUILD COMMAND ${CMAKE_COMMAND} -E
            copy_if_different "${DLL}" "$<TARGET_FILE_DIR:BankCLI>")
    endforeach()
endif()

option(BANKCLI_BUILD_TESTS "Build small framework-free tests" OFF)
if(BANKCLI_BUILD_TESTS)
    enable_testing()
    add_executable(BankCLITests tests.cpp Account.cpp Database.cpp BankService.cpp)
    target_include_directories(BankCLITests SYSTEM PRIVATE "${MYSQL_INCLUDE}")
    target_link_libraries(BankCLITests PRIVATE "${MYSQL_LIBRARY}")
    if(MSVC)
        target_compile_definitions(BankCLITests PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(WIN32)
        foreach(DLL IN LISTS MYSQL_DLLS)
            add_custom_command(TARGET BankCLITests POST_BUILD COMMAND ${CMAKE_COMMAND} -E
                copy_if_different "${DLL}" "$<TARGET_FILE_DIR:BankCLITests>")
        endforeach()
    endif()
    add_test(NAME account_rules COMMAND BankCLITests)
endif()
