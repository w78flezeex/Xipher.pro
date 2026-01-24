# Скрипты управления Xipher Server

Эта папка содержит скрипты для управления сервером Xipher.

## Доступные скрипты

### build.sh
Собирает проект из исходников.

```bash
./startup/build.sh
```

**Что делает:**
- Проверяет наличие зависимостей (CMake, libpq)
- Создает директорию `build/`
- Конфигурирует проект через CMake
- Собирает проект в Release режиме

### start.sh
Запускает сервер Xipher.

```bash
./startup/start.sh
```

**Что делает:**
- Проверяет наличие скомпилированного сервера
- Проверяет, не запущен ли уже сервер
- Проверяет доступность порта 21971
- (опционально) Применяет схему/миграции и бэкфилл владельцев каналов (см. `XIPHER_RUN_DB_FIX`)
- Запускает сервер в фоновом режиме
- Сохраняет PID в `/tmp/xipher_server.pid`
- Логи пишутся в `/var/log/xipher/server.log`

### fix_channel_owner.sh
Применяет схему/миграции и бэкфилл владельцев каналов (особенно важно после обновлений).

```bash
# По умолчанию использует DB_NAME=xipher (см. QUICK_START.md)
./startup/fix_channel_owner.sh

# Или явно
DB_NAME=xipher ./startup/fix_channel_owner.sh
```

### Авто‑прогон фикса БД при старте (опционально)
Если нужно гарантировать актуальные миграции/владельцев после деплоя:

```bash
XIPHER_RUN_DB_FIX=1 DB_NAME=xipher ./startup/start.sh
```

### stop.sh
Останавливает сервер Xipher.

```bash
./startup/stop.sh
```

**Что делает:**
- Останавливает процесс по PID файлу
- Ищет процессы по порту 21971
- Ищет процессы по имени `xipher_server`
- Очищает PID файл

### restart.sh
Перезапускает сервер (остановка + запуск).

```bash
./startup/restart.sh
```

### status.sh
Показывает статус сервера.

```bash
./startup/status.sh
```

**Показывает:**
- Статус процесса (запущен/остановлен)
- PID процесса
- Использование CPU и памяти
- Время работы
- Статус порта 21971
- Последние строки лога

## Примеры использования

### Первый запуск
```bash
# 1. Собрать проект
cd /root/xipher
./startup/build.sh

# 2. Запустить сервер
./startup/start.sh
```

### Обычная работа
```bash
# Запуск
./startup/start.sh

# Проверка статуса
./startup/status.sh

# Остановка
./startup/stop.sh

# Перезапуск
./startup/restart.sh
```

### После изменений в коде
```bash
# 1. Остановить сервер
./startup/stop.sh

# 2. Пересобрать
./startup/build.sh

# 3. Запустить
./startup/start.sh

# Или все сразу:
./startup/stop.sh && ./startup/build.sh && ./startup/start.sh
```

## Логи

Логи сервера находятся в: `/var/log/xipher/server.log`

Просмотр логов в реальном времени:
```bash
tail -f /var/log/xipher/server.log
```

## Запуск 24/7 + автозапуск при старте (systemd)

Самый надёжный способ — запускать `xipher_server` через `systemd` с `Restart=always`.

### Быстрая установка (рекомендуется)

```bash
cd /root/xipher
chmod +x ./src/startup/install_systemd.sh
./src/startup/install_systemd.sh
```

Если вы уже находитесь в `/root/xipher/src`, то можно так:

```bash
chmod +x ./startup/install_systemd.sh
./startup/install_systemd.sh
```

После этого отредактируйте `/etc/xipher/xipher.env` (порт/адрес/БД при необходимости) и перезапустите:

```bash
sudo systemctl restart xipher-server
sudo systemctl status xipher-server --no-pager
```

## Порт

По умолчанию сервер использует порт **21971**.

Для изменения порта отредактируйте переменную `PORT` в скриптах или передайте порт как аргумент при запуске сервера.

## Troubleshooting

### Порт занят
```bash
# Проверить, что занимает порт
lsof -i :21971
# или
ss -tlnp | grep 21971

# Остановить процесс
./startup/stop.sh
```

### Сервер не запускается
```bash
# Проверить логи
tail -50 /var/log/xipher/server.log

# Проверить статус
./startup/status.sh

# Проверить права доступа
ls -la /root/xipher/build/xipher_server
```

### Проблемы с БД
```bash
# Проверить подключение к БД
sudo -u postgres psql -d xipher -c "SELECT version();"
```
