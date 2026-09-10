#!/bin/sh


get_XDG_RUNTIME_DIR() {
  username_="$1"
  user_id_="$(get_user_id "$username_")"

  XDG_RUNTIME_DIR=""

  if command -v loginctl >/dev/null 2>&1; then
    XDG_RUNTIME_DIR=$(loginctl show-user "$username_" --property=RuntimePath 2>/dev/null | cut -d= -f2)
  fi

  # 2. Если loginctl не вернул значение, проверяем стандартный путь systemd
  if [ -z "$XDG_RUNTIME_DIR" ] && [ -d "/run/user/$user_id_" ]; then
    XDG_RUNTIME_DIR="/run/user/$user_id_"
  fi

  echo $XDG_RUNTIME_DIR
}


get_DBUS_SESSION_BUS_ADDRESS() {
  username_="$1"
  xdg_runtime_dir_="$2"
  user_id_="$(get_user_id "$username_")"

  DBUS_SESSION_BUS_ADDRESS=""

  # Способ А: Стандарт для современных systemd-систем (Ubuntu, Debian, РЕД ОС, ALT)
  # Ищем Unix-сокет, который systemd автоматически создает для пользователя
  if [ -S "$xdg_runtime_dir_/bus" ]; then
    DBUS_SESSION_BUS_ADDRESS="unix:path=$xdg_runtime_dir_/bus"
  # Способ Б: Поиск в окружении запущенных графических процессов пользователя
  # Идеально для старых версий или специфических графических окружений (например, некоторые сборки Fly в Astra Linux)
  elif [ -n "$user_id_" ]; then
    # Ищем процесс dbus-daemon или систему инициализации сессии пользователя
    dbus_pid_=$(pgrep -u "$user_id_" -x "dbus-daemon|dbus-launch|mate-session|xfce4-session|fly-wm" | head -n 1)
    if [ -n "$dbus_pid_" ]; then
      DBUS_SESSION_BUS_ADDRESS=$(grep -z '^DBUS_SESSION_BUS_ADDRESS=' /proc/"$dbus_pid_"/environ 2>/dev/null | cut -d= -f2- | tr -d '\0')
    fi
  fi

  # Способ В: Традиционный плоский файл-fallback (использовался в старых X11 сессиях)
  if [ -z "$DBUS_SESSION_BUS_ADDRESS" ]; then
    user_home_=$(getent passwd "$username_" | cut -d: -f6)
    machine_id_=$(cat /var/lib/dbus/machine-id 2>/dev/null || cat /etc/machine-id 2>/dev/null)

    if [ -n "$user_home_" ] && [ -n "$machine_id_" ]; then
      DBUS_FILE="$user_home_/.dbus/session-bus/$machine_id_-$(echo $DISPLAY | cut -d. -f1 | tr -d ':')"
      if [ -f "$DBUS_FILE" ]; then
        DBUS_SESSION_BUS_ADDRESS=$(grep '^DBUS_SESSION_BUS_ADDRESS=' "$DBUS_FILE" | cut -d= -f2- | tr -d "'\"")
      fi
    fi
  fi

  echo "${DBUS_SESSION_BUS_ADDRESS%%,*}"
}


get_active_regular_users() {
  for username in $(loginctl list-users --no-legend 2>/dev/null | awk '{print $2}'); do
    if getent passwd "$username" | cut -d: -f7 | grep -qf /etc/shells; then
      echo "$username"
    fi
  done
}


silent_run_systemctl_user_cmd() {
  username_="$1"
  shift

  XDG_RUNTIME_DIR="$( get_XDG_RUNTIME_DIR "$username_" )"
  DBUS_SESSION_BUS_ADDRESS="$( get_DBUS_SESSION_BUS_ADDRESS "$username_" "$XDG_RUNTIME_DIR" )"

  runuser -u "$username_" -- env \
    XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
    DBUS_SESSION_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS" \
    systemctl --machine="${username_}@" --user "$@" >/dev/null 2>&1 || true
}


stop_service_for_all_active_users() {
  service_name_="$1"
  for each_user in $( get_active_regular_users ); do
    silent_run_systemctl_user_cmd "${each_user}" stop "$service_name_"
  done
}


start_service_for_all_active_users() {
  service_name_="$1"
  for each_user in $( get_active_regular_users ); do
    silent_run_systemctl_user_cmd "${each_user}" daemon-reload
    silent_run_systemctl_user_cmd "${each_user}" enable --now "$service_name_"
  done
}


get_user_id() {
  id -u "$1" 2>/dev/null
}
