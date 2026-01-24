#!/usr/bin/env node
// Xipher Bot File Server - для VS Code/Cursor
// Монтирует файлы бота как локальную файловую систему
// Установите зависимости: npm install axios chokidar

const axios = require('axios');
const chokidar = require('chokidar');
const fs = require('fs');
const path = require('path');

const BOT_ID = '9a2e4ee8-1997-435e-8367-9b0ee9c58fb9';
const TOKEN = 'a5a792d9508463d37e804f5dc05620e432c9ed8ccb1b1809c1f794366b02c996';
const API_BASE = 'http://127.0.0.1:21971';
const LOCAL_DIR = path.join(process.cwd(), 'xipher-bot-' + BOT_ID.substring(0, 8));

console.log('🚀 Xipher Bot File Server');
console.log('📁 Папка проекта:', LOCAL_DIR);
console.log('');

// Создаем локальную папку
if (!fs.existsSync(LOCAL_DIR)) {
  fs.mkdirSync(LOCAL_DIR, { recursive: true });
  console.log('✓ Создана папка:', LOCAL_DIR);
}

// Функция для загрузки файла с сервера
async function downloadFile(filePath) {
  try {
    const response = await axios.post(`${API_BASE}/api/get-bot-file`, {
      token: TOKEN,
      bot_id: BOT_ID,
      file_path: filePath
    }, {
      timeout: 10000,
      headers: {
        'Content-Type': 'application/json'
      }
    });
    
    if (response.data && response.data.success && response.data.data) {
      const fullPath = path.join(LOCAL_DIR, filePath);
      const dir = path.dirname(fullPath);
      if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
      }
      const content = response.data.data.file_content || '';
      fs.writeFileSync(fullPath, content, 'utf8');
      return true;
    }
  } catch (error) {
    if (error.response && error.response.status === 404) {
      // Файл не найден на сервере - это нормально для новых файлов
      return false;
    }
    console.error(`✗ Ошибка загрузки ${filePath}:`, error.message);
  }
  return false;
}

// Функция для загрузки файла на сервер
async function uploadFile(filePath) {
  try {
    const fullPath = path.join(LOCAL_DIR, filePath);
    if (!fs.existsSync(fullPath)) {
      // Файл удален локально - удаляем на сервере
      try {
        await axios.post(`${API_BASE}/api/delete-bot-file`, {
          token: TOKEN,
          bot_id: BOT_ID,
          file_path: filePath
        });
        console.log(`🗑️  Удален на сервере: ${filePath}`);
      } catch (e) {
        // Игнорируем ошибки удаления
      }
      return;
    }
    
    // Проверяем, что это файл, а не директория
    const stats = fs.statSync(fullPath);
    if (!stats.isFile()) {
      return;
    }
    
    // Читаем файл с правильной кодировкой
    let content;
    try {
      content = fs.readFileSync(fullPath, 'utf8');
    } catch (e) {
      // Если не UTF-8, пробуем как бинарный (для изображений и т.д.)
      content = fs.readFileSync(fullPath, 'base64');
      // Для бинарных файлов пока не поддерживаем, пропускаем
      console.log(`⚠️  Бинарный файл пропущен: ${filePath}`);
      return;
    }
    
    const response = await axios.post(`${API_BASE}/api/save-bot-file`, {
      token: TOKEN,
      bot_id: BOT_ID,
      file_path: filePath.replace(/\\/g, '/'), // Нормализуем путь
      file_content: content
    }, {
      timeout: 10000,
      headers: {
        'Content-Type': 'application/json'
      }
    });
    
    if (response.data && response.data.success) {
      console.log(`✓ Синхронизирован: ${filePath}`);
      return true;
    } else {
      console.error(`✗ Ошибка ответа сервера для ${filePath}:`, response.data?.message || 'Unknown');
    }
  } catch (error) {
    if (error.code === 'ENOENT') {
      // Файл был удален, это нормально
      return;
    }
    console.error(`✗ Ошибка синхронизации ${filePath}:`, error.message);
  }
  return false;
}

// Загружаем все файлы с сервера при старте
async function syncFromServer() {
  console.log('🔄 Загрузка файлов с сервера...');
  console.log('📡 Подключение к API:', API_BASE);
  console.log('🤖 Bot ID:', BOT_ID);
  console.log('');
  
  try {
    const response = await axios.post(`${API_BASE}/api/list-bot-files`, {
      token: TOKEN,
      bot_id: BOT_ID
    }, {
      timeout: 15000,
      headers: {
        'Content-Type': 'application/json'
      }
    });
    
    if (response.data && response.data.success && response.data.files) {
      const files = response.data.files;
      console.log(`📦 Найдено файлов на сервере: ${files.length}`);
      console.log('');
      
      if (files.length === 0) {
        console.log('ℹ️  На сервере нет файлов. Создайте файлы в веб-IDE или через VS Code.');
        console.log('');
        return;
      }
      
      let downloaded = 0;
      let errors = 0;
      
      for (const file of files) {
        const success = await downloadFile(file.file_path);
        if (success) {
          downloaded++;
          process.stdout.write(`
📥 Загружено: ${downloaded}/${files.length} файлов`);
        } else {
          errors++;
        }
      }
      
      console.log('');
      console.log('');
      console.log(`✅ Загружено ${downloaded} из ${files.length} файлов`);
      if (errors > 0) {
        console.log(`⚠️  Ошибок: ${errors}`);
      }
      console.log('');
    } else {
      console.error('✗ Неверный ответ сервера:', response.data);
    }
  } catch (error) {
    if (error.code === 'ECONNREFUSED') {
      console.error('✗ Не удалось подключиться к серверу:', API_BASE);
      console.error('   Убедитесь, что сервер Xipher запущен');
    } else if (error.response) {
      console.error('✗ Ошибка API:', error.response.status, error.response.data?.message || error.message);
    } else {
      console.error('✗ Ошибка загрузки:', error.message);
    }
    console.error('');
    console.error('Проверьте:');
    console.error('  1. Сервер Xipher запущен на', API_BASE);
    console.error('  2. Токен и Bot ID правильные');
    console.error('  3. Интернет-соединение работает');
    console.error('');
  }
}

// Отслеживаем изменения в локальных файлах
function watchFiles() {
  console.log('👀 Отслеживание изменений файлов...');
  const watcher = chokidar.watch(LOCAL_DIR, {
    ignored: [/node_modules/, /\.git/, /\.vscode/],
    persistent: true,
    ignoreInitial: true,
    awaitWriteFinish: {
      stabilityThreshold: 500,
      pollInterval: 100
    }
  });
  
  watcher.on('change', async (filePath) => {
    const relativePath = path.relative(LOCAL_DIR, filePath);
    await uploadFile(relativePath);
  });
  
  watcher.on('add', async (filePath) => {
    const relativePath = path.relative(LOCAL_DIR, filePath);
    await uploadFile(relativePath);
  });
  
  watcher.on('unlink', async (filePath) => {
    const relativePath = path.relative(LOCAL_DIR, filePath);
    await uploadFile(relativePath); // uploadFile обработает удаление
  });
  
  watcher.on('error', (error) => {
    console.error('Ошибка watcher:', error);
  });
}

// Периодическая синхронизация с сервера (каждые 30 секунд)
let syncInterval = null;
function startPeriodicSync() {
  syncInterval = setInterval(async () => {
    try {
      const response = await axios.post(`${API_BASE}/api/list-bot-files`, {
        token: TOKEN,
        bot_id: BOT_ID
      });
      
      if (response.data.success && response.data.files) {
        // Проверяем, есть ли новые файлы на сервере
        for (const file of response.data.files) {
          const localPath = path.join(LOCAL_DIR, file.file_path);
          if (!fs.existsSync(localPath)) {
            // Файл есть на сервере, но нет локально - загружаем
            await downloadFile(file.file_path);
          }
        }
      }
    } catch (error) {
      // Игнорируем ошибки периодической синхронизации
    }
  }, 30000); // 30 секунд
}

// Запуск
(async () => {
  await syncFromServer();
  watchFiles();
  startPeriodicSync();
  
  console.log('');
  console.log('═══════════════════════════════════════════════════════');
  console.log('✅ Файловый сервер запущен!');
  console.log('');
  console.log('📂 Откройте эту папку в VS Code/Cursor:');
  console.log('   ' + LOCAL_DIR);
  console.log('');
  console.log('💡 Инструкция:');
  console.log('   1. File → Open Folder → выберите папку выше');
  console.log('   2. Редактируйте файлы - изменения синхронизируются автоматически');
  console.log('   3. Создавайте новые файлы - они сразу появятся на сервере');
  console.log('   4. Не закрывайте этот терминал');
  console.log('');
  console.log('🛑 Для остановки нажмите Ctrl+C');
  console.log('═══════════════════════════════════════════════════════');
  console.log('');
})();

// Graceful shutdown
process.on('SIGINT', () => {
  console.log('\n🛑 Остановка сервера...');
  if (syncInterval) clearInterval(syncInterval);
  process.exit(0);
});