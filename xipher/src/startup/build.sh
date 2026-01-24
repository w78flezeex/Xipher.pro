#!/bin/bash
# Скрипт для сборки проекта Xipher

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
for _ in 1 2 3 4; do
    PROJECT_DIR="$(dirname "$PROJECT_DIR")"
    if [ -f "$PROJECT_DIR/CMakeLists.txt" ]; then
        break
    fi
done
if [ ! -f "$PROJECT_DIR/CMakeLists.txt" ]; then
    echo "ОШИБКА: Не удалось найти корень проекта (CMakeLists.txt) от SCRIPT_DIR=$SCRIPT_DIR" >&2
    exit 1
fi
BUILD_DIR="$PROJECT_DIR/build"

echo "=========================================="
echo "  Xipher - Сборка проекта"
echo "=========================================="
echo ""

cd "$PROJECT_DIR"

# Проверка зависимостей
echo "[1/4] Проверка зависимостей..."
if ! command -v cmake &> /dev/null; then
    echo "ОШИБКА: CMake не установлен"
    exit 1
fi

if ! pkg-config --exists libpq; then
    echo "ОШИБКА: libpq не найден"
    exit 1
fi

echo "✓ Зависимости проверены"
echo ""

# Создание директории build
echo "[2/4] Подготовка директории build..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
echo "✓ Директория готова"
echo ""

# Конфигурация CMake
echo "[3/4] Конфигурация CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release
if [ $? -ne 0 ]; then
    echo "ОШИБКА: Конфигурация CMake не удалась"
    exit 1
fi
echo "✓ Конфигурация завершена"
echo ""

# Сборка
echo "[4/4] Сборка проекта..."
JOBS="${XIPHER_BUILD_JOBS:-}"
if [ -z "$JOBS" ]; then
    JOBS=$(nproc 2>/dev/null || echo 2)
    if [ -r /proc/meminfo ]; then
        MEM_KB=$(awk '/MemTotal/ {print $2}' /proc/meminfo)
        if [ -n "$MEM_KB" ]; then
            if [ "$MEM_KB" -lt 8000000 ]; then
                JOBS=1
            elif [ "$MEM_KB" -lt 16000000 ]; then
                JOBS=2
            elif [ "$MEM_KB" -lt 32000000 ] && [ "$JOBS" -gt 4 ]; then
                JOBS=4
            fi
        fi
    fi
fi
echo "Используем потоков: $JOBS"
make -j"$JOBS"
if [ $? -ne 0 ]; then
    echo "ОШИБКА: Сборка не удалась"
    exit 1
fi
echo "✓ Сборка завершена"
echo ""

echo "=========================================="
echo "  Сборка успешно завершена!"
echo "  Исполняемый файл: $BUILD_DIR/xipher_server"
echo "=========================================="
