cmake_minimum_required(VERSION 3.15)

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


# Подключаем определение переменных с названиями системных
# директорий размещения бинарников (bin/, lib/ и т.п.)
#
include(GNUInstallDirs)


# Правила сборки и инсталляции основного бинарника проекта
#
add_executable(custom_service main.cpp)
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


# Минимальные настройки для формирования deb-пакета (пока только для Debian-систем)
#
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${PROJECT_AUTHOR_NAME} <${PROJECT_AUTHOR_EMAIL}>")
set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
     "${CMAKE_CURRENT_SOURCE_DIR}/postinst"
     "${CMAKE_CURRENT_SOURCE_DIR}/prerm"
)

include(CPack)
