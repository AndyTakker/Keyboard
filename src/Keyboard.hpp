//============================================================= (с) A.Kolesov ==
// Keyboard.hpp
// Библиотека для работы с клавиатурой.
// - проверяет нажатие кнопки (под нажатием понимается сигнал высокого или низкого уровня
//   на опрашиваемом порте/пине, в зависимости от конфигурации кнопки).
// - устраняет дребезг.
// - обеспечивает одновременное нажатие нескольких кнопок.
// - обеспечивает проверку долгого нажатия кнопки (врменной порог задается в конфигурации кнопки).
// - при нажатии кнопки вызывает обработчик, в котором можно реализовать логику обработки
//   нажатий кнопок.
// - содержит функцию автоматической инициализации портов, на которых опрашиваются кнопки.
//
// При инициализации класса нужно передать указатель на функцию, которая будет
// возвращать текущее время в миллисекундах. Можно ипользовать функцию Sysclock.Millis
// из библиотеки SysClock или любую другую.
//------------------------------------------------------------------------------
#pragma once

#include <Logs.h>
#include <ch32Pins.hpp>
#include <ch32v00x_gpio.h>
#include <string.h>

struct KeyConfig {                          // Конфигурация одной опрашиваемой кнопки
  PinName pinName;                          // Пин, на котором опрашиваемая кнопка
  BitAction activeLevel;                    // Bit_RESET или Bit_SET: уровень, который считается нажатием
  const char *name;                         // Название кнопки.
  uint32_t holdTimeMs;                      // Порог долгого нажатия (мс)
  GPIOMode_TypeDef pinMode = GPIO_Mode_IPU; // Режим пина по умолчанию
};

// Класс для передачи параметров в обработчик нажатия кнопки.
class KeyEvent {
  public:
  const char *name;       // Имя кнопки
  bool isLongPress;       // Флаг долгого нажатия
  uint32_t pressDuration; // фактическое время удержания
};

struct KeyStatus {        // Структура для запроса состояний кнопок
  const char *name;       // Имя кнопки
  bool isPressed;         // устойчивое состояние (после debounce)
  bool isLongPress;       // удерживается дольше holdTimeMs
  uint32_t pressDuration; // если pressed == true
};

// Обработчик нажатий. Вызывается для каждой нажатой кнопки, т.е. если одновременно нажаты
// несколько кнопок, то обработчик будет вызван для каждой нажатой кнопки.
using KeyCallback = void (*)(const KeyEvent &);

template <size_t N>
class Keyboard {
  public:
  Keyboard(const KeyConfig (&keys)[N], uint32_t (*getMillis)(), bool autoInit = true)
      : m_keys(keys), m_getMillis(getMillis) {
    // Если установлен флаг автоинициализации, то инициализируем порты, на которых кнопки
    if (autoInit) {
      for (size_t i = 0; i < N; ++i) {
        pinMode(m_keys[i].pinName, m_keys[i].pinMode);
      }
    }
  };

  bool update(); // Функция опроса и обновления состояния кнопок. Должна вызываться максимально часто (не реже интервала DEBOUNCE).
  void setDebounce(uint32_t debounceMs) { m_debounceMs = debounceMs; };
  void setCallback(KeyCallback cb) { m_callback = cb; }

  bool isPressed(const char *keyName) const {
    for (size_t i = 0; i < N; ++i) {
      if (m_keys[i].name && keyName && strcmp(m_keys[i].name, keyName) == 0) {
        return m_states[i].pressed;
      }
    }
    return false;
  }

  // Очистить состояние кнопок
  void clear() {
    for (size_t i = 0; i < N; ++i) {
      m_states[i].pressed = false;
      m_states[i].pressTime = 0;
      lastRead = 0;
    }
  }

  // Функция возвращает статус всех опрашиваемых кнопок
  void getStatus(KeyStatus (&out)[N]) const {
    uint32_t now = m_getMillis();
    for (size_t i = 0; i < N; ++i) {
      out[i].name = m_keys[i].name;
      out[i].isPressed = m_states[i].pressed;
      if (m_states[i].pressed) {
        out[i].pressDuration = now - m_states[i].pressTime;
        out[i].isLongPress = (out[i].pressDuration >= m_keys[i].holdTimeMs);
      } else {
        out[i].pressDuration = 0;
        out[i].isLongPress = false;
      }
    }
  }

  private:
  struct KeyState {     // Состояние одной кнопки
    bool pressed;       // Предыдущее состояние
    uint32_t pressTime; // Момент нажатия
  };

  const KeyConfig *m_keys;          // Указатель на массив конфигурации кнопок
  KeyState m_states[N] = {};        // Массив состояний опрашиваемых кнопок
  uint32_t (*m_getMillis)();        // Функция получения текущего времени
  KeyCallback m_callback = nullptr; // Обработчик нажатий
  uint32_t m_debounceMs = 20;       // Задержка антидребезга в ms.
  uint32_t lastRead = 0;            // Время последнего опроса клавиатуры
};

//==============================================================================
// Функция опроса и обновления состояния кнопок.
// При обнаружении отпускания нажатой кнопки вызывается обработчик нажатий, если
// он установлен.
// Возвращает true, если в процессе вызова было изменено состояние любой клавиши.
//------------------------------------------------------------------------------
template <size_t N>
bool Keyboard<N>::update() {
  bool anyKeyChanged = false;

  uint32_t now = m_getMillis();
  if (now - lastRead < m_debounceMs) // Опрашиваем клавиатуру с периодичностью подавления дребезга
    return false;
  lastRead = now;

  // Логика опроса клавиатуры:
  // Проходим по списку клавиш и читаем состояние каждой. Если клавиша не нажата (т.е. отпущена),
  // проверяем ее состояние в предыдущем опросе. Если оно изменилось, значит действительно,
  // клавиша была нажата и отпущена. Значит ее код нужно вернуть.
  for (size_t i = 0; i < N; ++i) { // Перебираем все кнопки
    const auto &cfg = m_keys[i];   // Конфигурация текущей кнопки
    KeyState &state = m_states[i]; // Состояние текущей кнопки

    // Чтение текущего состояния в терминах нажата/отпущена
    bool raw = (pinRead(cfg.pinName) == cfg.activeLevel);

    // Текущее состояние - нажата, а предыдущее - не нажата, фиксируем факт первого нажатия
    if (raw && !state.pressed) {
      state.pressed = raw;   // Фиксируем факт нажатия
      state.pressTime = now; // и время нажатия
      anyKeyChanged = true;
      continue; // К следующей кнопке
    }

    // Была нажата, но сейчас отпущена
    if (!raw && state.pressed) {
      state.pressed = raw;                        // Фиксируем факт отпускания
      uint32_t duration = now - state.pressTime;  // Время удержания
      bool isLong = (duration >= cfg.holdTimeMs); // Флаг долгого нажатия
      anyKeyChanged = true;
      if (m_callback) {
        m_callback({cfg.name, isLong, duration});
      }
      continue; // К следующей кнопке
    }
  }
  return anyKeyChanged;
};
