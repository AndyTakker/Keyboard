//============================================================= (c) A.Kolesov ==
// Пример использования библиотеки Keyboard.
// Сценарии использования:
// 1. Простой, когда нужно опрашивать небольшое количество одиночно нажимаемых
//    кнопок и взводить флаги для их последующей обработки или выполнять быстрые,
//    короткие действия.
//    В этом случае создаем объект Keyboard и указываем обработчик нажатий.
//    В обработчике устанавливаем необходимые признаки.
//    !!! Обработчик должен выполняться быстро, т.к. во время работы обработчика
//    состояние других клавиш не анализируется.
//    Кроме всего прочего, наличие обработчика позволяет эмулировать нажатие
//    кнопок из кода программы. Например: onKeyEvent({ "UP", true, 1200 });
//    Важно!!! Обработчик вызывается в момент отпускания кнопки.
// 2. Когда требуется обрабатывает комбинации нажатий и удержаний пары кнопок.
//    В этом случае анализируем дополнительно состояние связанных клавиш,
//    в обработчике методом Keyboard::isPressed().
// 3. Когда нужны сложные комбинации нажатых, отпущенных, удерживаемых кнопок.
//    Тогда нужно пользоваться методом Keyboard::getState(). Он позволяет
//    получить текущее состояние всех опрашиваемых кнопок в деталях и затем
//    анализировать их комбинации.
//------------------------------------------------------------------------------
#include <Keyboard.hpp>
#include <Logs.h>
#include <SysClock.h>
#include <debug.h>

enum class KeyId : uint8_t { UP = 0,
                             DOWN };

// Конфигурация опрашиваемых кнопок
static const KeyConfig keys[] = {
    {PD3, Bit_RESET, (uint8_t)KeyId::UP, 1000},
    {PD2, Bit_RESET, (uint8_t)KeyId::DOWN, 800}};

const uint8_t countKeys = sizeof(keys) / sizeof(keys[0]);         // Количество опрашиваемых кнопок
static Keyboard<countKeys> keyboard(keys, Sysclock.Millis, true); // Создаем экземпляр Keyboard с указанными кнопками

// Пример обработчика нажатий кнопок. Обработчика может не быть.
void onKeyEvent(const KeyEvent &e) {
  // Для указанной кнопки проверяем, как долго она была нажата
  if (e.isLongPress) {
    logs("LONG (%lu ms): %d\n", e.pressDuration, e.id);
  } else {
    logs("TAP (%lu ms): %d\n", e.pressDuration, e.id);
  }
  // А это пример проверки на модификатор, при отпускании интересующей нас кнопки
  // не была ли нажата клавиша-модификатор (Shift, Alt, Ctrl)
  // (в данном примере модификатор - это UP)
  if (keyboard.isPressed((uint8_t)KeyId::UP)) {
    logs("UP_pressed\n");
  }
}

//==============================================================================
int main() {
  SystemCoreClockUpdate();
#ifdef LOG_ENABLE
  USART_Printf_Init(115200);
#endif
  logs("SystemClk: %lu\r\n", SystemCoreClock);        // Для посмотреть частоту процесора (48мГц)
  logs("   ChipID: 0x%08lX\r\n", DBGMCU_GetCHIPID()); // Для посмотреть ID чипа, от нефиг делать

  keyboard.setCallback(onKeyEvent); // Подключить обработчик
  keyboard.clear();                 // Очистить состояние кнопок. Все будут ненажаты. Это для примера.
  while (1) {
    if (keyboard.update()) { // Опрашиваем клавиатуру
      logs("Keyboard update\r\n");

      // Пример ниже можно использовать вместо коллбека или вместе с ним.
      // для более изощренных обработок
      auto statuses = keyboard.getStatus();

      // Анализируем текущее состояние всех кнопок (уже с учетом антидребезга)
      // Пример-1: одновременное нажатие UP + DOWN
      bool upPressed = false, downPressed = false;
      for (auto &s : statuses) {
        if (s.id == (uint8_t)KeyId::UP)
          upPressed = s.isPressed;
        if (s.id == (uint8_t)KeyId::DOWN)
          downPressed = s.isPressed;
      }
      if (upPressed && downPressed) {
        logs("UP+DOWN pressed\r\n");
      }
      // Пример-2: долгое удержание DOWN во время нажатия UP
      bool downHeldLong = false;
      for (auto &s : statuses) {
        if (s.id == (uint8_t)KeyId::DOWN)
          downHeldLong = s.isLongPress;
      }
      if (upPressed && downHeldLong) {
        logs("UP+DOWN held long\r\n");
      }
    }
    // Делаем что-то нужное
  }
}
