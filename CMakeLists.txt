cmake_minimum_required(VERSION 3.15)

project(
     custom_service
     LANGUAGES CXX
     VERSION 1.0.0
     DESCRIPTION "Simple syncronous inverting socket server"
)

set(PROJECT_AUTHOR_NAME "Енакаев Алексей")
set(PROJECT_AUTHOR_EMAIL "Enakaev.Aleksey@iitrust.ru")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -O0 -fno-inline")

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
     set(CMAKE_INSTALL_PREFIX /opt/${PROJECT_NAME} CACHE PATH "Project install prefix" FORCE)
endif()

add_executable(custom_service main.cpp)

include(GNUInstallDirs)
install(TARGETS custom_service
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${PROJECT_AUTHOR_NAME} <${PROJECT_AUTHOR_EMAIL}>")
set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

include(CPack)
