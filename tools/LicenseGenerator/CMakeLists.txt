cmake_minimum_required(VERSION 3.16)

project(LicenseGenerator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_executable(LicenseGenerator
    main.cpp
)

target_link_libraries(LicenseGenerator PRIVATE Qt6::Core Qt6::Widgets)

install(TARGETS LicenseGenerator
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
