cmake_minimum_required(VERSION 3.15)

project(custom_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -O0 -fno-inline")

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
     set(CMAKE_INSTALL_PREFIX /opt/${PROJECT_NAME})
endif()

add_executable(custom_service main.cpp)

include(GNUInstallDirs)
install(TARGETS custom_service
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
