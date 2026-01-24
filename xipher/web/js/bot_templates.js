// Lightweight bot templates (25+). Each template maps to a flat JSON config stored in Bot Builder flow_json.
// IMPORTANT: Keep configs as flat key/value string pairs (server JsonParser is flat).

(function () {
  const T = [
    {
      id: 'automod_basic',
      title: 'AutoMod Lite',
      category: 'Модерация',
      scopes: ['group'],
      description: 'Блокирует ссылки, капс и плохие слова. Предупреждает в группе.',
      bot_name: 'AutoMod Lite Bot',
      config: {
        template_id: 'automod_basic',
        module_moderation: 'true',
        mod_block_links: 'true',
        mod_block_caps: 'true',
        mod_caps_min_len: '12',
        mod_block_words: 'true',
        mod_bad_words: 'сука,бля,хуй,пизд',
        mod_auto_mute: 'false',
        mod_warn_text: '⚠️ {reason}'
      }
    },
    {
      id: 'automod_strict',
      title: 'AutoMod Strict',
      category: 'Модерация',
      scopes: ['group'],
      description: 'Как Lite, но может авто-мутить нарушителей.',
      bot_name: 'AutoMod Strict Bot',
      config: {
        template_id: 'automod_strict',
        module_moderation: 'true',
        mod_block_links: 'true',
        mod_block_caps: 'true',
        mod_caps_min_len: '10',
        mod_block_words: 'true',
        mod_bad_words: 'сука,бля,хуй,пизд,нахуй',
        mod_auto_mute: 'true',
        mod_warn_text: '⚠️ {reason}'
      }
    },
    {
      id: 'anti_link',
      title: 'Anti‑Link',
      category: 'Модерация',
      scopes: ['group'],
      description: 'Запрещает любые ссылки в группе.',
      bot_name: 'Anti‑Link Bot',
      config: {
        template_id: 'anti_link',
        module_moderation: 'true',
        mod_block_links: 'true',
        mod_block_caps: 'false',
        mod_block_words: 'false',
        mod_auto_mute: 'false',
        mod_warn_text: '⚠️ Ссылки запрещены'
      }
    },
    {
      id: 'anti_caps',
      title: 'Anti‑Caps',
      category: 'Модерация',
      scopes: ['group'],
      description: 'Просит не кричать капсом (в группе).',
      bot_name: 'Anti‑Caps Bot',
      config: {
        template_id: 'anti_caps',
        module_moderation: 'true',
        mod_block_links: 'false',
        mod_block_caps: 'true',
        mod_caps_min_len: '12',
        mod_block_words: 'false',
        mod_auto_mute: 'false',
        mod_warn_text: '⚠️ Не кричи капсом'
      }
    },
    {
      id: 'anti_swear',
      title: 'Anti‑Swear',
      category: 'Модерация',
      scopes: ['group'],
      description: 'Следит за матом и предупреждает (в группе).',
      bot_name: 'Anti‑Swear Bot',
      config: {
        template_id: 'anti_swear',
        module_moderation: 'true',
        mod_block_links: 'false',
        mod_block_caps: 'false',
        mod_block_words: 'true',
        mod_bad_words: 'сука,бля,хуй,пизд,нахуй',
        mod_auto_mute: 'false',
        mod_warn_text: '⚠️ Нецензурные слова запрещены'
      }
    },
    {
      id: 'welcome_basic',
      title: 'Welcome Bot',
      category: 'Комьюнити',
      scopes: ['group'],
      description: 'Приветствует новых участников, когда они заходят по инвайту.',
      bot_name: 'Welcome Bot',
      config: {
        template_id: 'welcome_basic',
        module_welcome: 'true',
        welcome_text: 'Привет, @{username}! Добро пожаловать 👋'
      }
    },
    {
      id: 'rules_basic',
      title: 'Rules Bot',
      category: 'Комьюнити',
      scopes: ['dm', 'group'],
      description: 'Команда /rules показывает правила.',
      bot_name: 'Rules Bot',
      config: {
        template_id: 'rules_basic',
        module_rules: 'true',
        rules_text: 'Правила:\\n1) Уважение\\n2) Без спама\\n3) Без ссылок\\n\\nКоманды: /help'
      }
    },
    {
      id: 'community_manager',
      title: 'Community Manager',
      category: 'Комьюнити',
      scopes: ['group'],
      description: 'Welcome + Rules + AutoMod Lite (универсальный бот для чата).',
      bot_name: 'Community Manager Bot',
      config: {
        template_id: 'community_manager',
        module_welcome: 'true',
        welcome_text: 'Привет, @{username}! Правила: /rules',
        module_rules: 'true',
        rules_text: 'Правила:\\n1) Без токсичности\\n2) Без рекламы\\n3) Оффтоп — в треды',
        module_moderation: 'true',
        mod_block_links: 'true',
        mod_block_caps: 'true',
        mod_caps_min_len: '12',
        mod_block_words: 'true',
        mod_bad_words: 'сука,бля,хуй,пизд',
        mod_auto_mute: 'false',
        mod_warn_text: '⚠️ {reason}'
      }
    },
    {
      id: 'notes_group',
      title: 'Notes Bot',
      category: 'Утилиты',
      scopes: ['dm', 'group'],
      description: 'Заметки для группы: /note, /notes, /delnote.',
      bot_name: 'Notes Bot',
      config: {
        template_id: 'notes_group',
        module_notes: 'true'
      }
    },
    {
      id: 'notes_personal',
      title: 'Personal Notes',
      category: 'ЛС',
      scopes: ['dm'],
      description: 'Личные заметки в ЛС с ботом.',
      bot_name: 'Personal Notes Bot',
      config: {
        template_id: 'notes_personal',
        module_notes: 'true',
        dm_default_reply: 'Напиши /help или сохрани заметку: /note ключ текст'
      }
    },
    {
      id: 'reminder_bot',
      title: 'Reminder Bot',
      category: 'ЛС',
      scopes: ['dm', 'group'],
      description: 'Напоминания: /remind 10m текст (в ЛС и в группе).',
      bot_name: 'Reminder Bot',
      config: {
        template_id: 'reminder_bot',
        module_remind: 'true'
      }
    },
    {
      id: 'study_helper',
      title: 'Study Helper',
      category: 'ЛС',
      scopes: ['dm'],
      description: 'Заметки + напоминания (идеально для учёбы).',
      bot_name: 'Study Helper Bot',
      config: {
        template_id: 'study_helper',
        module_notes: 'true',
        module_remind: 'true',
        dm_default_reply: 'Я помогу: /note и /remind'
      }
    },
    {
      id: 'team_todo',
      title: 'Team TODO',
      category: 'Утилиты',
      scopes: ['dm', 'group'],
      description: 'Заметки + напоминания для команды.',
      bot_name: 'Team TODO Bot',
      config: {
        template_id: 'team_todo',
        module_notes: 'true',
        module_remind: 'true',
        module_rules: 'true',
        rules_text: 'Команды:\\n/notes\\n/note key text\\n/remind 1h text'
      }
    },
    {
      id: 'fun_bot',
      title: 'Fun Bot',
      category: 'Развлечения',
      scopes: ['dm', 'group'],
      description: 'Рандом и выбор: /roll, /coin, /choose.',
      bot_name: 'Fun Bot',
      config: {
        template_id: 'fun_bot',
        module_fun: 'true'
      }
    },
    {
      id: 'dice_bot',
      title: 'Dice Bot',
      category: 'Развлечения',
      scopes: ['dm', 'group'],
      description: 'Только /roll (для игр).',
      bot_name: 'Dice Bot',
      config: {
        template_id: 'dice_bot',
        module_fun: 'true',
        dm_default_reply: 'Напиши /roll 20 или /coin'
      }
    },
    {
      id: 'support_autoreply',
      title: 'Support Auto‑Reply',
      category: 'ЛС',
      scopes: ['dm'],
      description: 'Авто‑ответы по ключевым словам в ЛС (поддержка).',
      bot_name: 'Support Bot',
      config: {
        template_id: 'support_autoreply',
        module_autoreply: 'true',
        autoreply_rules: 'привет=Привет! Опиши проблему одним сообщением;оплата=По оплате напиши номер заказа;бан=Если бан — скинь скрин',
        dm_default_reply: 'Напиши /help или ключевое слово (например: оплата)'
      }
    },
    {
      id: 'sales_autoreply',
      title: 'Sales Auto‑Reply',
      category: 'ЛС',
      scopes: ['dm'],
      description: 'Авто‑ответы для продаж/вопросов.',
      bot_name: 'Sales Bot',
      config: {
        template_id: 'sales_autoreply',
        module_autoreply: 'true',
        autoreply_rules: 'цена=Прайс вышлю в ответ;доставка=Доставка 1–3 дня;скидка=Скидки от 10 шт',
        dm_default_reply: 'Спроси про цену/доставку/скидку'
      }
    },
    {
      id: 'onboarding_bot',
      title: 'Onboarding',
      category: 'Комьюнити',
      scopes: ['group'],
      description: 'Welcome + rules + авто‑ответ на “как начать”.',
      bot_name: 'Onboarding Bot',
      config: {
        template_id: 'onboarding_bot',
        module_welcome: 'true',
        welcome_text: 'Привет, @{username}! Начни с /rules и /help',
        module_rules: 'true',
        rules_text: 'Начало:\\n1) Представься\\n2) Прочитай /rules\\n3) Задавай вопросы',
        module_autoreply: 'true',
        autoreply_rules: 'как начать=Начни с /rules и напиши чем занимаешься'
      }
    },
    {
      id: 'rules_fun',
      title: 'Rules + Fun',
      category: 'Комьюнити',
      scopes: ['dm', 'group'],
      description: 'Правила и развлечения.',
      bot_name: 'Rules & Fun Bot',
      config: {
        template_id: 'rules_fun',
        module_rules: 'true',
        rules_text: 'Правила простые: будь нормальным 🙂',
        module_fun: 'true'
      }
    },
    {
      id: 'study_group_mod',
      title: 'Study Group Mod',
      category: 'Модерация',
      scopes: ['group'],
      description: 'Мягкая модерация + заметки + правила.',
      bot_name: 'Study Group Bot',
      config: {
        template_id: 'study_group_mod',
        module_moderation: 'true',
        mod_block_links: 'true',
        mod_block_caps: 'true',
        mod_block_words: 'false',
        mod_auto_mute: 'false',
        module_notes: 'true',
        module_rules: 'true',
        rules_text: 'Учёба:\\n- без оффтопа\\n- ссылки только по теме'
      }
    },
    {
      id: 'clean_chat',
      title: 'Clean Chat',
      category: 'Модерация',
      scopes: ['group'],
      description: 'Запрещает ссылки + мат (мягко).',
      bot_name: 'Clean Chat Bot',
      config: {
        template_id: 'clean_chat',
        module_moderation: 'true',
        mod_block_links: 'true',
        mod_block_caps: 'false',
        mod_block_words: 'true',
        mod_bad_words: 'сука,бля,хуй,пизд',
        mod_auto_mute: 'false'
      }
    },
    {
      id: 'strict_clean_chat',
      title: 'Strict Clean Chat',
      category: 'Модерация',
      scopes: ['group'],
      description: 'Запрещает ссылки + мат и авто‑мутит.',
      bot_name: 'Strict Clean Chat Bot',
      config: {
        template_id: 'strict_clean_chat',
        module_moderation: 'true',
        mod_block_links: 'true',
        mod_block_caps: 'false',
        mod_block_words: 'true',
        mod_bad_words: 'сука,бля,хуй,пизд,нахуй',
        mod_auto_mute: 'true'
      }
    },
    {
      id: 'office_bot',
      title: 'Office Bot',
      category: 'Утилиты',
      scopes: ['dm', 'group'],
      description: 'Заметки + напоминания + rules (для рабочего чата).',
      bot_name: 'Office Bot',
      config: {
        template_id: 'office_bot',
        module_notes: 'true',
        module_remind: 'true',
        module_rules: 'true',
        rules_text: 'Рабочие правила:\\n- без флуда\\n- задачи фиксируем /note'
      }
    },
    {
      id: 'party_bot',
      title: 'Party Bot',
      category: 'Развлечения',
      scopes: ['dm', 'group'],
      description: 'Fun + авто‑ответы (например “играем?”).',
      bot_name: 'Party Bot',
      config: {
        template_id: 'party_bot',
        module_fun: 'true',
        module_autoreply: 'true',
        autoreply_rules: 'играем=Го! /choose cs|valorant|dota;музыка=Скинь трек',
        dm_default_reply: 'Пиши /help'
      }
    },
    {
      id: 'quick_helper',
      title: 'Quick Helper',
      category: 'ЛС',
      scopes: ['dm'],
      description: 'Мини‑бот: /help + /rules + /note.',
      bot_name: 'Quick Helper Bot',
      config: {
        template_id: 'quick_helper',
        module_rules: 'true',
        rules_text: 'Мини‑помощник. Команды: /help',
        module_notes: 'true'
      }
    },
    {
      id: 'pomodoro_dm',
      title: 'Pomodoro',
      category: 'ЛС',
      scopes: ['dm'],
      description: 'Фокус‑таймер через /remind (пример: /remind 25m помодоро).',
      bot_name: 'Pomodoro Bot',
      config: {
        template_id: 'pomodoro_dm',
        module_remind: 'true',
        dm_default_reply: 'Пример: /remind 25m помодоро • /remind 5m перерыв'
      }
    },
    {
      id: 'shopping_list',
      title: 'Shopping List',
      category: 'ЛС',
      scopes: ['dm'],
      description: 'Список покупок на заметках: /note молоко 2л, /notes.',
      bot_name: 'Shopping List Bot',
      config: {
        template_id: 'shopping_list',
        module_notes: 'true',
        dm_default_reply: 'Сохраняй: /note item молоко • Смотри: /notes'
      }
    },
    {
      id: 'habit_tracker',
      title: 'Habit Tracker',
      category: 'ЛС',
      scopes: ['dm'],
      description: 'Заметки + напоминания для привычек.',
      bot_name: 'Habit Tracker Bot',
      config: {
        template_id: 'habit_tracker',
        module_notes: 'true',
        module_remind: 'true',
        dm_default_reply: 'Пример: /note привычка вода 8 стаканов • /remind 2h выпей воды'
      }
    },
    {
      id: 'faq_autoreply',
      title: 'FAQ Auto‑Reply',
      category: 'ЛС',
      scopes: ['dm'],
      description: 'FAQ‑автоответы по ключевым словам.',
      bot_name: 'FAQ Bot',
      config: {
        template_id: 'faq_autoreply',
        module_autoreply: 'true',
        autoreply_rules: 'время=Мы отвечаем с 10:00 до 20:00;цена=Цены на сайте;контакты=Напиши сюда или на почту',
        dm_default_reply: 'Спроси: время / цена / контакты'
      }
    }
  ];

  window.XIPHER_BOT_TEMPLATES = T;
})();


