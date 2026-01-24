# Быстрый старт - Xipher Server

## Первый запуск

```bash
cd /root/xipher

# 1. Собрать проект
./startup/build.sh

# 1.1 (рекомендуется) Применить схему/миграции и бэкфилл владельцев каналов
DB_NAME=xipher ./startup/fix_channel_owner.sh

# 2. Запустить сервер
./startup/start.sh
```

## Управление сервером

```bash
# Запуск
./startup/start.sh

# Остановка
./startup/stop.sh

# Перезапуск
./startup/restart.sh

# Статус
./startup/status.sh
```

## После изменений в коде

```bash
./startup/stop.sh
./startup/build.sh
./startup/start.sh
```

## Проверка работы

```bash
# Проверить, что сервер отвечает
curl http://127.0.0.1:21971/

# Тест регистрации
curl -X POST http://127.0.0.1:21971/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"testpass123"}'

# Тест входа
curl -X POST http://127.0.0.1:21971/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"testpass123"}'
```

## Логи

```bash
# Просмотр логов
tail -f /var/log/xipher/server.log
```

## База данных

- **База данных:** xipher
- **Пользователь:** xipher
- **Пароль:** xipher
- **Порт:** 5432 (PostgreSQL)

## Порт сервера

По умолчанию: **21971**

Сервер доступен на: `http://127.0.0.1:21971`

Через Nginx: `https://xipher.pro`
