#include "button_driver.h"
#include "stm32f10x.h"
#include "system_time.h"
#include "error_manager.h"
#include "debug_logger.h"

#define GPIO_BUTTON_PORT             GPIOB
#define GPIO_BUTTON_UP_PIN           GPIO_Pin_12
#define GPIO_BUTTON_DOWN_PIN         GPIO_Pin_13
#define GPIO_BUTTON_SELECT_PIN       GPIO_Pin_14
#define GPIO_BUTTON_CHARGE_PIN       GPIO_Pin_15
#define GPIO_BUTTON_DEBOUNCE_MS      20U
#define GPIO_BUTTON_EVENT_QUEUE_SIZE 8U
#define GPIO_BUTTON_INDEX_IS_VALID(i) ((uint8_t)(i) < (uint8_t)BUTTON_COUNT)

static void ButtonDriver_Clear_Events(void);

/**
 * @brief Button hardware configuration.
 */
typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
} ButtonDriver_Init_t;

/**
 * @brief Runtime debounce state for one active-low button.
 */
typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
  uint8_t raw_state;
  uint8_t stable_state;
  uint32_t raw_changed_tick;
} Button_t;

static Button_t buttons[BUTTON_COUNT];

/* UP/DOWN/SELECT are always available. CHARGE is enabled only in capacitor mode. */
static uint8_t gpio_button_enabled_flags[BUTTON_COUNT] = {
  1U, 1U, 1U, 0U
};

/* Small FIFO so more than one debounced press cannot overwrite another. */
static ButtonEvent_t event_queue[GPIO_BUTTON_EVENT_QUEUE_SIZE];
static uint8_t event_head = 0U;
static uint8_t event_tail = 0U;
static uint8_t event_count = 0U;

static const ButtonDriver_Init_t gpio_button_init_once[BUTTON_COUNT] = {
  { GPIO_BUTTON_PORT, GPIO_BUTTON_UP_PIN },
  { GPIO_BUTTON_PORT, GPIO_BUTTON_DOWN_PIN },
  { GPIO_BUTTON_PORT, GPIO_BUTTON_SELECT_PIN },
  { GPIO_BUTTON_PORT, GPIO_BUTTON_CHARGE_PIN },
};

static ButtonEvent_t ButtonDriver_Event_From_ID(uint8_t index)
{
  switch (index) {
    case BUTTON_UP:
      return BUTTON_EVENT_UP_PRESSED;
    case BUTTON_DOWN:
      return BUTTON_EVENT_DOWN_PRESSED;
    case BUTTON_SELECT:
      return BUTTON_EVENT_SELECT_PRESSED;
    case BUTTON_CHARGE:
      return BUTTON_EVENT_CHARGE_PRESSED;
    default:
      return BUTTON_EVENT_NONE;
  }
}

static uint8_t ButtonDriver_ID_From_Event(ButtonEvent_t event)
{
  switch (event) {
    case BUTTON_EVENT_UP_PRESSED:
      return BUTTON_UP;
    case BUTTON_EVENT_DOWN_PRESSED:
      return BUTTON_DOWN;
    case BUTTON_EVENT_SELECT_PRESSED:
      return BUTTON_SELECT;
    case BUTTON_EVENT_CHARGE_PRESSED:
      return BUTTON_CHARGE;
    default:
      return BUTTON_COUNT;
  }
}

static void ButtonDriver_Push_Event(ButtonEvent_t event)
{
  if (event == BUTTON_EVENT_NONE) {
    return;
  }

  if (event_count >= GPIO_BUTTON_EVENT_QUEUE_SIZE) {
    ErrorManager_Report(ERROR_SOURCE_BUTTON,
                        ERROR_CODE_BUTTON_QUEUE_FULL,
                        ERROR_SEVERITY_WARNING);
    return;
  }

  event_queue[event_tail] = event;
  event_tail = (uint8_t)((event_tail + 1U) % GPIO_BUTTON_EVENT_QUEUE_SIZE);
  event_count++;
}

/**
 * @brief Remove queued events belonging to one button.
 * @note  Used when a mode disables a context-sensitive button such as CHARGE.
 */
static void ButtonDriver_Remove_Events(uint8_t index)
{
  ButtonEvent_t keep[GPIO_BUTTON_EVENT_QUEUE_SIZE];
  uint8_t keep_count = 0U;

  while (event_count > 0U) {
    ButtonEvent_t event = event_queue[event_head];
    event_head = (uint8_t)((event_head + 1U) % GPIO_BUTTON_EVENT_QUEUE_SIZE);
    event_count--;

    if (ButtonDriver_ID_From_Event(event) != index) {
      keep[keep_count++] = event;
    }
  }

  event_head = 0U;
  event_tail = 0U;

  for (uint8_t i = 0U; i < keep_count; ++i) {
    event_queue[event_tail] = keep[i];
    event_tail = (uint8_t)((event_tail + 1U) % GPIO_BUTTON_EVENT_QUEUE_SIZE);
  }

  event_count = keep_count;
}

static void ButtonDriver_ConfigPin(GPIO_TypeDef *GPIOx,
                   uint16_t GPIO_Pin,
                   GPIOMode_TypeDef GPIO_Mode,
                   GPIOSpeed_TypeDef GPIO_Speed)
{
  GPIO_InitTypeDef GPIO_InitStructure;

  if (GPIOx == GPIOA) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
  } else if (GPIOx == GPIOB) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
  } else if (GPIOx == GPIOC) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
  }

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed;
  GPIO_Init(GPIOx, &GPIO_InitStructure);
}

static void ButtonDriver_Init_Once(Button_t *button,
                                  const ButtonDriver_Init_t *init,
                                  uint32_t now)
{
  uint8_t state;

  button->port = init->port;
  button->pin = init->pin;

  state = (GPIO_ReadInputDataBit(button->port, button->pin) != Bit_RESET) ? 1U : 0U;

  button->raw_state = state;
  button->stable_state = state;
  button->raw_changed_tick = now;
}

/**
 * @brief Update one button using a 20 ms time-based debounce.
 *
 * A raw transition starts a debounce timer. Only if that raw level remains
 * unchanged for GPIO_BUTTON_DEBOUNCE_MS is it accepted as the new stable level.
 * A press event is emitted only on the stable HIGH -> LOW edge.
 */
static void ButtonDriver_Update_Once(uint8_t index, uint32_t now)
{
  Button_t *button = &buttons[index];
  uint8_t raw_state =
      (GPIO_ReadInputDataBit(button->port, button->pin) != Bit_RESET) ? 1U : 0U;

  if (raw_state != button->raw_state) {
    button->raw_state = raw_state;
    button->raw_changed_tick = now;
    return;
  }

  if ((button->stable_state != button->raw_state) &&
      ((uint32_t)(now - button->raw_changed_tick) >= GPIO_BUTTON_DEBOUNCE_MS)) {

    button->stable_state = button->raw_state;

    /* Buttons use internal pull-up, therefore stable LOW means pressed. */
    if ((button->stable_state == 0U) && gpio_button_enabled_flags[index]) {
      ButtonDriver_Push_Event(ButtonDriver_Event_From_ID(index));
    }
  }
}

void ButtonDriver_Init(void)
{
  uint32_t now;

  ButtonDriver_ConfigPin(GPIO_BUTTON_PORT,
                GPIO_BUTTON_UP_PIN |
                GPIO_BUTTON_DOWN_PIN |
                GPIO_BUTTON_SELECT_PIN |
                GPIO_BUTTON_CHARGE_PIN,
                GPIO_Mode_IPU,
                GPIO_Speed_50MHz);

  now = SystemTime_GetTick();

  for (uint8_t i = 0U; i < BUTTON_COUNT; ++i) {
    ButtonDriver_Init_Once(&buttons[i], &gpio_button_init_once[i], now);
  }

  ButtonDriver_Clear_Events();
}

void ButtonDriver_Process(void)
{
  uint32_t now = SystemTime_GetTick();

  for (uint8_t i = 0U; i < BUTTON_COUNT; ++i) {
    ButtonDriver_Update_Once(i, now);
  }
}

ButtonEvent_t ButtonDriver_GetEvent(void)
{
  ButtonEvent_t event;

  if (event_count == 0U) {
    return BUTTON_EVENT_NONE;
  }

  event = event_queue[event_head];
  event_head = (uint8_t)((event_head + 1U) % GPIO_BUTTON_EVENT_QUEUE_SIZE);
  event_count--;

  return event;
}

static void ButtonDriver_Clear_Events(void)
{
  event_head = 0U;
  event_tail = 0U;
  event_count = 0U;
}

void ButtonDriver_Enable(ButtonId_t index)
{
  if (!GPIO_BUTTON_INDEX_IS_VALID(index)) {
    return;
  }

  /* Remove a stale event before accepting new presses in this mode. */
  ButtonDriver_Remove_Events(index);
  gpio_button_enabled_flags[index] = 1U;

  if (index == BUTTON_CHARGE) {
    DebugLogger_Log(DEBUG_LEVEL_TRACE, "BUTTON", "CHARGE enabled");
  }
}

void ButtonDriver_Disable(ButtonId_t index)
{
  if (!GPIO_BUTTON_INDEX_IS_VALID(index)) {
    return;
  }

  gpio_button_enabled_flags[index] = 0U;
  ButtonDriver_Remove_Events(index);

  if (index == BUTTON_CHARGE) {
    DebugLogger_Log(DEBUG_LEVEL_TRACE, "BUTTON", "CHARGE disabled");
  }
}


