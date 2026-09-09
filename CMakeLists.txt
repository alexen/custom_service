cmake_minimum_required(VERSION 3.15..3.31)

project(
     custom_service
          LANGUAGES CXX
          VERSION 1.0.0
          DESCRIPTION "Simple syncronous inverting socket server"
)

set(PROJECT_AUTHOR_NAME "Enakaev Aleksey")
set(PROJECT_AUTHOR_EMAIL "enakaev.aleksey@yandex.ru")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -O0 -fno-inline")


# Устанавливаем префикс инсталляции по умолчанию в "/opt/<AUTHOR_NICK>/<PROJECT_NAME>/"
#
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
     set(CMAKE_INSTALL_PREFIX "/opt/alexen/${PROJECT_NAME}"
          CACHE PATH "Default project install prefix" FORCE)
endif()


if(POLICY CMP0167)
     cmake_policy(SET CMP0167 OLD)
endif()

set(Boost_NO_BOOST_CMAKE ON)
find_package(Boost COMPONENTS log REQUIRED)
find_package(Threads REQUIRED)


# Подключаем определение переменных с названиями системных
# директорий размещения бинарников (bin/, lib/ и т.п.)
#
include(GNUInstallDirs)


# Правила сборки и инсталляции основного бинарника проекта
#
add_executable(custom_service main.cpp)
target_compile_definitions(
     custom_service
     PRIVATE
          BOOST_LOG_DYN_LINK
)
target_link_libraries(
     custom_service
     PRIVATE
          Boost::log
          Threads::Threads
)
install(TARGETS custom_service
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)


# Правила формирования и установки unit-файла сервиса
#
set(SERVICE_UNIT_INSTALL_PREFIX "/usr/lib/systemd/user")
set(SERVICE_UNIT_FILE_TEMPLATE custom_service.service.in)
set(SERVICE_UNIT_OUTPUT_FILE "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.service")

configure_file(
     "${SERVICE_UNIT_FILE_TEMPLATE}"
     "${SERVICE_UNIT_OUTPUT_FILE}"
     @ONLY
)

install(
     FILES "${SERVICE_UNIT_OUTPUT_FILE}"
     DESTINATION "${SERVICE_UNIT_INSTALL_PREFIX}"
     PERMISSIONS
          OWNER_READ OWNER_WRITE
          GROUP_READ
          WORLD_READ
)


configure_file("${CMAKE_CURRENT_LIST_DIR}/preinst.in"  "${CMAKE_BINARY_DIR}/preinst"  @ONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/postinst.in" "${CMAKE_BINARY_DIR}/postinst" @ONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/prerm.in"    "${CMAKE_BINARY_DIR}/prerm"    @ONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/postrm.in"   "${CMAKE_BINARY_DIR}/postrm"   @ONLY)

# Универсальные настройки для всех видов пакетов
#
set(CPACK_PACKAGE_VENDOR "${PROJECT_AUTHOR_NAME}")
set(CPACK_PACKAGE_CONTACT "${PROJECT_AUTHOR_EMAIL}")

# Определяем тип целевого пакета по информации о системе
#
cmake_host_system_information(RESULT DISTRO QUERY DISTRIB_INFO)

# В некоторых системах отсутствует переменная ID_LIKE, есть только переменная ID.
# В связи с этим необходимо список DISTRO_ID_LIKE формировать из двух переменных.
#
string(REPLACE " " ";" DISTRO_ID_LIST "${DISTRO_ID_LIKE} ${DISTRO_ID}")

if("debian" IN_LIST DISTRO_ID_LIST)
     set(CPACK_GENERATOR "DEB")
elseif("rhel" IN_LIST DISTRO_ID_LIST)
     set(CPACK_GENERATOR "RPM")
elseif("altlinux" IN_LIST DISTRO_ID_LIST)
     set(CPACK_GENERATOR "RPM")
else()
     message(FATAL_ERROR "Unrecognized target OS: ${DISTRO_ID}")
endif()

message(STATUS "Installation package type: ${CPACK_GENERATOR}")

# Минимальные настройки для формирования deb-пакета
#
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR} <${CPACK_PACKAGE_CONTACT}>")
set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
     "${CMAKE_BINARY_DIR}/preinst"
     "${CMAKE_BINARY_DIR}/postinst"
     "${CMAKE_BINARY_DIR}/prerm"
     "${CMAKE_BINARY_DIR}/postrm"
)

# Минимальные настройки для сборки rpm-пакета
#
set(CPACK_RPM_PRE_INSTALL_SCRIPT_FILE    "${CMAKE_BINARY_DIR}/preinst")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE   "${CMAKE_BINARY_DIR}/postinst")
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE  "${CMAKE_BINARY_DIR}/prerm")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_BINARY_DIR}/postrm")

include(CPack)
