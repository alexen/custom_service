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

# Включаем FetchContent
include(FetchContent)

# Объявляем зависимость от супер-проекта Boost
FetchContent_Declare(
    Boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.85.0/boost-1.85.0-cmake.tar.xz
    URL_HASH SHA256=0a9cc56ceae46986f5f4d43fe0311d90cf6d2fa9028258a95cab49ffdacf92ad
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(BOOST_ENABLE_CMAKE ON CACHE INTERNAL "")
# Отключаем тесты адресно для Boost
set(BOOST_STANDALONE OFF CACHE INTERNAL "")

set(BOOST_BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(BOOST_INCLUDE_LIBRARIES log CACHE INTERNAL "")

# Загружаем Boost в дерево сборки
FetchContent_MakeAvailable(Boost)
find_package(Threads REQUIRED)


# Подключаем определение переменных с названиями системных
# директорий размещения бинарников (bin/, lib/ и т.п.)
#
include(GNUInstallDirs)


# Правила сборки и инсталляции основного бинарника проекта
#
add_executable(custom_service main.cpp)
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
set(SERVICE_UNIT_OUTPUT_FILE "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.service")

configure_file(
     "${CMAKE_CURRENT_LIST_DIR}/custom_service.service.in"
     "${SERVICE_UNIT_OUTPUT_FILE}"
     @ONLY
)

install(
     FILES "${SERVICE_UNIT_OUTPUT_FILE}"
     DESTINATION "/usr/lib/systemd/user"
     PERMISSIONS
          OWNER_READ OWNER_WRITE
          GROUP_READ
          WORLD_READ
)

# Встраиваем sh-файл с вспомогательными функциями в контрольные скрипты deb и rpm
# (при этом отмечаем внешний файл с функциями как часть проекта, чтобы CMake отслеживал
# его состояние и обновлял результирующие (выходные) файлы при любом изменении содержимого)
#
set_property(DIRECTORY APPEND PROPERTY
     CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/tools.sh"
)
file(READ "${CMAKE_CURRENT_LIST_DIR}/tools.sh" TOOLS_SH_CONTENT)

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

# Зачищаем формирование списка зависимостей RPM-пакета
set(CPACK_RPM_PACKAGE_AUTOREQ     OFF)
set(CPACK_RPM_PACKAGE_AUTOREQPROV OFF) # Отключение стандартных зависимостей
set(CPACK_RPM_DEBUGINFO_PACKAGE   OFF) # Отключение CPack debug
set(CPACK_RPM_COMPRESSION_TYPE "gzip") # Убирает PayloadIsLzma
set(CPACK_RPM_SPEC_MORE_DEFINE "%global debug_package %{nil}") # Убирает debug64
# Жестко запрещаем rpmbuild запускать скрипт поиска отладочной информации
set(CPACK_RPM_SPEC_INSTALL_POST "/bin/true")
# Настраиваем регулярное выражение, чтобы RPM отфильтровал и выбросил любые упоминания debug
set(CPACK_RPM_SPEC_MORE_DEFINE "
%define __requires_exclude_from ^/usr/lib/debug/.*$
%define __requires_exclude ^(debug64.*|.*/ld-linux-x86-64.so.2.debug)$
%global debug_package %{nil}
")

include(CPack)
