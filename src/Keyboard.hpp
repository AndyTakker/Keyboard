//============================================================= (с) A.Kolesov ==
// Keyboard.hpp
// Библиотека для работы с клавиатурой.
// - проверяет нажатие кнопки (под нажатием понимается сигнал высокого или низкого уровня
//   на опрашиваемом порте/пине, в зависимости от конфигурации кнопки).
// - устраняет дребезг.
// - обеспечивает одновременное нажатие нескольких кнопок.
// - обеспечивает проверку долгого нажатия кнопки (врменной порог задается в конфигурации кнопки).
// - при ОТПУСКАНИИ нажатой кнопки вызывает обработчик, в котором можно реализовать логику обработки
//   нажатий кнопок.
// - содержит функцию автоматической инициализации портов, на которых опрашиваются кнопки.
//
// При инициализации класса нужно передать указатель на функцию, которая будет
// возвращать текущее время в миллисекундах. Можно ипользовать функцию Sysclock.Millis
// из библиотеки SysClock или любую другую.
// 
// Добавлена подержка Arduino-framework.
//------------------------------------------------------------------------------
#pragma once

#include <Logs.h>
#include <string.h>

#ifndef ARDUINO
#include <array>
#include <ch32Pins.hpp>
#include <ch32v00x_gpio.h>
#else
#include <Arduino.h>
#define PinName uint8_t
#define BitAction bool
#define Bit_RESET false
#define Bit_SET true
#define pinRead(x) digitalRead(x)
#endif

struct KeyConfig { // Конфигурация одной опрашиваемой кнопки
#ifndef ARDUINO
  PinName pinName;       // Пин, на котором опрашиваемая кнопка
  BitAction activeLevel; // Bit_RESET или Bit_SET: уровень, который считается нажатием
#else
  uint8_t pinName;  // Пин, на котором опрашиваемая кнопка
  bool activeLevel; // Bit_RESET или Bit_SET: уровень, который считается нажатием
#endif
  uint8_t id;          // Уникальный идентификатор кнопки.
  uint32_t holdTimeMs; // Порог долгого нажатия (мс)
#ifndef ARDUINO
  GPIOMode_TypeDef pinMode = GPIO_Mode_IPU; // Режим пина по умолчанию
#else
  uint8_t pinMode; // = INPUT_PULLUP; // Режим пина по умолчанию для arduino не поддерживается
#endif
};

// Класс для передачи параметров в обработчик нажатия кнопки.
class KeyEvent {
  public:
  uint8_t id;             // Уникальный идентификатор кнопки.
  bool isLongPress;       // Флаг долгого нажатия
  uint32_t pressDuration; // Фактическое время удержания
};

struct KeyStatus {        // Структура для запроса состояний кнопок
  uint8_t id;             // Уникальный идентификатор кнопки
  bool isPressed;         // устойчивое состояние (после debounce)
  bool isLongPress;       // удерживается дольше holdTimeMs
  uint32_t pressDuration; // если pressed == true
};

// Обработчик нажатий. Вызывается для каждой нажатой кнопки, т.е. если одновременно нажаты
// несколько кнопок, то обработчик будет вызван для каждой нажатой кнопки.
using KeyCallback = void (*)(const KeyEvent &);

template <size_t N>
class Keyboard {
  private:
  // Создаем сразу массив для состояний всех кнопок и его будем по запросу
  // заполнять и возвращать (для не-ардуино).
#ifndef ARDUINO
  std::array<KeyStatus, N> statuses_;
#else
  KeyStatus statuses[N];
#endif
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

  bool isPressed(uint8_t keyId) const {
    for (size_t i = 0; i < N; ++i) {
      if (m_keys[i].id == keyId) {
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
    }
    lastRead = 0;
  }

#ifndef ARDUINO
  // Функция возвращает статус всех опрашиваемых кнопок
  std::array<KeyStatus, N> getStatus() {
    uint32_t now = m_getMillis();
    for (uint8_t i = 0; i < statuses_.size(); ++i) {
      statuses_[i].id = m_keys[i].id;
      statuses_[i].isPressed = m_states[i].pressed;
      if (m_states[i].pressed) {
        statuses_[i].pressDuration = now - m_states[i].pressTime;
        statuses_[i].isLongPress = (statuses_[i].pressDuration >= m_keys[i].holdTimeMs);
      } else {
        statuses_[i].pressDuration = 0;
        statuses_[i].isLongPress = false;
      }
    }
    return statuses_;
  }
#endif

  // Функция возвращает статус всех опрашиваемых кнопок
  // Аналог предыдущей функции, но заполняет данными полученный в параметре массив.
  // Менее удобная, но мало-ли.
  void getStatus(KeyStatus (&out)[N]) const {
    uint32_t now = m_getMillis();
    for (size_t i = 0; i < N; ++i) {
      out[i].id = m_keys[i].id;
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
    // Чтение текущего состояния в терминах нажата/отпущена
    bool raw = (pinRead(m_keys[i].pinName) == m_keys[i].activeLevel);
    // Текущее состояние - нажата, а предыдущее - не нажата, фиксируем факт первого нажатия
    if (raw && !m_states[i].pressed) {
      m_states[i].pressed = raw;   // Фиксируем факт нажатия
      m_states[i].pressTime = now; // и время нажатия
      anyKeyChanged = true;
      continue; // К следующей кнопке
    }

    // Была нажата, но сейчас отпущена
    if (!raw && m_states[i].pressed) {
      m_states[i].pressed = raw;                        // Фиксируем факт отпускания
      uint32_t duration = now - m_states[i].pressTime;  // Время удержания
      bool isLong = (duration >= m_keys[i].holdTimeMs); // Флаг долгого нажатия
      anyKeyChanged = true;
      if (m_callback) {
        m_callback({m_keys[i].id, isLong, duration});
      }
      continue; // К следующей кнопке
    }
  }
  return anyKeyChanged;
};
