cmake_minimum_required(VERSION 3.16)
project(BankCLI LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MYSQL_CONNECTOR_ROOT "" CACHE PATH "Folder containing MySQL Connector/C++")
find_path(MYSQL_INCLUDE mysql_driver.h
    HINTS "${MYSQL_CONNECTOR_ROOT}/include/jdbc" "${MYSQL_CONNECTOR_ROOT}/include")
find_library(MYSQL_LIBRARY NAMES mysqlcppconn
    HINTS "${MYSQL_CONNECTOR_ROOT}/lib64/vs14" "${MYSQL_CONNECTOR_ROOT}/lib64"
          "${MYSQL_CONNECTOR_ROOT}/lib")
if(NOT MYSQL_INCLUDE OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "Set MYSQL_CONNECTOR_ROOT to your Connector/C++ folder. See README.md.")
endif()

# Compile the banking code once; both the menu and tests use it.
add_library(bankcli_core Database.cpp BankService.cpp)
target_include_directories(bankcli_core SYSTEM PUBLIC "${MYSQL_INCLUDE}")
target_link_libraries(bankcli_core PUBLIC "${MYSQL_LIBRARY}")

add_executable(BankCLI main.cpp)
target_link_libraries(BankCLI PRIVATE bankcli_core)
set(BANKCLI_TARGETS bankcli_core BankCLI)
set(BANKCLI_PROGRAMS BankCLI)

option(BANKCLI_BUILD_TESTS "Build the tests" ON)
if(BANKCLI_BUILD_TESTS)
    enable_testing()
    find_package(Threads REQUIRED)
    add_executable(BankCLITests tests.cpp)
    target_link_libraries(BankCLITests PRIVATE bankcli_core Threads::Threads)
    list(APPEND BANKCLI_TARGETS BankCLITests)
    list(APPEND BANKCLI_PROGRAMS BankCLITests)
    add_test(NAME account_rules COMMAND BankCLITests)
endif()

foreach(TARGET_NAME IN LISTS BANKCLI_TARGETS)
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /W4 /permissive-)
        target_compile_definitions(${TARGET_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

# Windows needs the connector DLL and its supporting DLLs beside each program.
if(WIN32)
    file(GLOB MYSQL_DLLS "${MYSQL_CONNECTOR_ROOT}/lib64/*.dll"
                        "${MYSQL_CONNECTOR_ROOT}/bin/*.dll")
    foreach(PROGRAM IN LISTS BANKCLI_PROGRAMS)
        foreach(DLL IN LISTS MYSQL_DLLS)
            add_custom_command(TARGET ${PROGRAM} POST_BUILD COMMAND ${CMAKE_COMMAND} -E
                copy_if_different "${DLL}" "$<TARGET_FILE_DIR:${PROGRAM}>")
        endforeach()
    endforeach()
endif()
