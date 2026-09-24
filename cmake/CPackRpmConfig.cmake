set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Минимальные настройки для сборки rpm-пакета
#
set(CPACK_RPM_PRE_INSTALL_SCRIPT_FILE    "${CMAKE_BINARY_DIR}/preinst")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE   "${CMAKE_BINARY_DIR}/postinst")
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE  "${CMAKE_BINARY_DIR}/prerm")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_BINARY_DIR}/postrm")

# ==============================================================================
# 1. УРОВЕНЬ CPACK: Полное отключение встроенного поиска CMake
# ==============================================================================
set(CPACK_RPM_PACKAGE_AUTOREQ     "no")
set(CPACK_RPM_PACKAGE_AUTOPROV    "no")
set(CPACK_RPM_PACKAGE_AUTOREQPROV "no")
set(CPACK_RPM_COMPRESSION_TYPE    "gzip")
set(CPACK_RPM_PACKAGE_REQUIRES    "")

# ==============================================================================
# 2. УРОВЕНЬ RPMBUILD: Тотальное подавление генераторов на всех дистрибутивах
# ==============================================================================
set(CPACK_RPM_SPEC_MORE_DEFINE "
# ==============================================================================
# ОБЩИЕ МАКРОСЫ (Безопасны для всех систем, включая РЕД ОС)
# ==============================================================================
%define _use_internal_dependency_generator 0
%global _use_internal_dependency_generator 0
%define __find_requires %{nil}
%global __find_requires %{nil}
%define __find_provides %{nil}
%global __find_provides %{nil}

# Жесткая подмена путей к скриптам генерации
%define __find_requires /bin/true
%define __find_provides /bin/true

# Отключение терминации сборки из-за документации или скрытых файлов
%define _missing_doc_files_terminate_build 0
%define _unpackaged_files_terminate_build 0

# --- Специфичные макросы для РЕД ОС / RHEL / Fedora / Mageia ---
# Если это НЕ ALT Linux, используем фильтрацию регулярными выражениями (вырезаем всё)
%if ! 0%{?altlinux}
%define __requires_exclude .*
%global __requires_exclude .*
%define __provides_exclude .*
%global __provides_exclude .*
%define _requires_exceptions .*
%define _provides_exceptions .*
%endif

# ==============================================================================
# СПЕЦИФИЧНЫЕ МАКРОСЫ ДЛЯ ALT LINUX (Изолированы условием)
# ==============================================================================
%if 0%{?altlinux}
# Фильтруем абсолютно все зависимости
%define __find_requires_filter /.*
%global __find_requires_filter /.*
%define __find_provides_filter /.*
%global __find_provides_filter /.*
# Вносим все файлы и скрипты в черный список для проверки
%set_findreq_skiplist *
%set_findprov_skiplist *
%endif
")
