#include "gpio.h"
#include <stdio.h>

/**
 * @brief Enabled flags for each button.
 */
static volatile uint8_t gpio_button_enabled_flags[GPIO_BUTTON_NUMBERS] = { 0 };

/**
 * @brief Pressed-once flags for each button.
 */
static volatile uint8_t gpio_button_pressed_flags[GPIO_BUTTON_NUMBERS] = { 0 };

/**
 * @brief Button hardware configuration (GPIO port and pin).
 */
typedef struct
{
  GPIO_TypeDef *port;  /**< GPIO port */
  uint16_t pin;        /**< GPIO pin */
} Gpio_Button_Init_t;


/**
 * @brief Runtime status of a button with debouncing information.
 */
typedef struct
{
  GPIO_TypeDef *port;      /**< GPIO port */
  uint16_t pin;            /**< GPIO pin */

  uint8_t last_stable;     /**< Last stable logic level */
  uint8_t current_stable;  /**< Current stable logic level */
  uint8_t pressed_once;    /**< One-shot press flag */
  uint8_t sample_count;    /**< Number of collected samples */
  uint64_t sample_log;     /**< Sampling history log */
} Button_t;

/**
 * @brief Array holding the runtime states of all buttons.
 */
static Button_t buttons[GPIO_BUTTON_NUMBERS];

/**
 * @brief Initial hardware configuration for all buttons.
 */
const Gpio_Button_Init_t gpio_button_init_once[GPIO_BUTTON_NUMBERS] = {
  { GPIO_BUTTON_PORT, GPIO_BUTTON_UP_PIN },
  { GPIO_BUTTON_PORT, GPIO_BUTTON_DOWN_PIN },
  { GPIO_BUTTON_PORT, GPIO_BUTTON_SELECT_PIN },
  { GPIO_BUTTON_PORT, GPIO_BUTTON_CHARGE_PIN },
};

/**
 * @brief Initialize a GPIO pin.
 *
 * @param GPIOx       GPIO port (GPIOA, GPIOB, ...)
 * @param GPIO_Pin    Pin number
 * @param GPIO_Mode   Pin mode (input/output)
 * @param GPIO_Speed  Output speed
 */
void Gpio_Init_Pin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIOMode_TypeDef GPIO_Mode, GPIOSpeed_TypeDef GPIO_Speed) {
  /* Enable clock for the given GPIO port */
  if (GPIOx == GPIOA) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
  } else if (GPIOx == GPIOB) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
  } else if (GPIOx == GPIOC) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
  }
  // Extend with other ports if necessary

  /* Configure GPIO pin */
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed;
  GPIO_Init(GPIOx, &GPIO_InitStructure);
}

/**
 * @brief Initialize the runtime state of a single button.
 *
 * @param gpio_button Pointer to the button runtime structure
 * @param init        Pointer to the hardware configuration
 */
static void Gpio_Button_Init_Once(Button_t *gpio_button, const Gpio_Button_Init_t *init) {
  gpio_button->port = init->port;
  gpio_button->pin = init->pin;

  gpio_button->last_stable = 1;
  gpio_button->current_stable = 1;
  gpio_button->pressed_once = 0;
  gpio_button->sample_count = 0;
  gpio_button->sample_log = 0xFFFFFFFFFFFFFFFF;
}

/**
 * @brief Update the debounced state of a single button.
 *
 * Performs sampling, debouncing and press-detection.
 *
 * @param gpio_button       Pointer to the button runtime structure
 * @param gpio_pressed_flag Pointer to the pressed flag to be updated
 */
static void Gpio_Button_Update_Once(Button_t *gpio_button, volatile uint8_t *gpio_pressed_flag) {
  uint8_t button_input_value = GPIO_ReadInputDataBit(gpio_button->port, gpio_button->pin);

  gpio_button->sample_log = gpio_button->sample_log << 1;

  if (button_input_value != 0) {
    gpio_button->sample_log |= 1;
  }

  if (gpio_button->sample_count < GPIO_BUTTON_SAMPLING_TIMES) {
    gpio_button->sample_count++;
  } else {
    uint8_t count_logic_high = 0;
    for (uint8_t i = 0; i < GPIO_BUTTON_SAMPLING_TIMES; i++) {
      if ((gpio_button->sample_log >> i) & 0x01) {
        count_logic_high++;
      }
    }

    uint8_t new_stable = (count_logic_high * 100 / GPIO_BUTTON_SAMPLING_TIMES) > GPIO_BUTTON_HIGH_PERCENT;

    if (new_stable == 0 && gpio_button->last_stable == 1) {
      gpio_button->pressed_once = 1;
    } else {
      gpio_button->pressed_once = 0;
    }

    gpio_button->last_stable = new_stable;
    gpio_button->current_stable = new_stable;

    if (gpio_button->pressed_once) {
      *gpio_pressed_flag = 1; /**< Update pressed flag */
    }
  }
}

/**
 * @brief Initialize all buttons.
 */
void Gpio_Button_Init_All(void) {
  Gpio_Init_Pin(
    GPIO_BUTTON_PORT,
    GPIO_BUTTON_UP_PIN | GPIO_BUTTON_DOWN_PIN | GPIO_BUTTON_SELECT_PIN | GPIO_BUTTON_CHARGE_PIN,
    GPIO_Mode_IPU,
    GPIO_Speed_50MHz);

  for (uint8_t i = 0; i < GPIO_BUTTON_NUMBERS; ++i) {
    Gpio_Button_Init_Once(&buttons[i], &gpio_button_init_once[i]);
  }
}

/**
 * @brief Update the state of all buttons.
 */
void Gpio_Button_Update_All(void) {
  for (uint8_t i = 0; i < GPIO_BUTTON_NUMBERS; i++) {
    Gpio_Button_Update_Once(&buttons[i], &gpio_button_pressed_flags[i]);
  }
}

/**
 * @brief Reset the pressed flag of a single button.
 *
 * @param index Button index
 */
inline void Gpio_Button_Reset_Once(uint8_t index) {
  if (!GPIO_BUTTON_INDEX_IS_VALID(index)) {
    return;
  }

  gpio_button_pressed_flags[index] = 0;
}

/**
 * @brief Reset the pressed flag of all buttons.
 */
void Gpio_Button_Reset_All(void) {
  for (uint8_t i = 0; i < GPIO_BUTTON_NUMBERS; i++) {
    Gpio_Button_Reset_Once(i);
  }
}

/**
 * @brief Enable a button.
 *
 * @param index Button index
 */
void Gpio_Button_Enable(uint8_t index) {
  if (!GPIO_BUTTON_INDEX_IS_VALID(index)) {
    return;
  }

  gpio_button_enabled_flags[index] = 1;

#ifdef GPIO_BUTTON_DEBUG
  if (index == BUTTON_CHARGE) {
    printf("Button Charge Enable!\r\n");
  }
#endif
}

/**
 * @brief Disable a button.
 *
 * @param index Button index
 */
void Gpio_Button_Disable(uint8_t index) {
  if (!GPIO_BUTTON_INDEX_IS_VALID(index)) {
    return;
  }

  gpio_button_enabled_flags[index] = 0;

#ifdef GPIO_BUTTON_DEBUG
  if (index == BUTTON_CHARGE) {
    printf("Button Charge Disable!\r\n");
  }
#endif
}

/**
 * @brief Check if a button is enabled.
 *
 * @param index Button index
 * @retval 1 if enabled, 0 otherwise
 */
inline uint8_t Gpio_Button_Is_Enabled(uint8_t index) {
  if (!GPIO_BUTTON_INDEX_IS_VALID(index)) {
#ifdef GPIO_BUTTON_DEBUG
    printf("Invalid Button Index: %d\r\n", index);
#endif
    return 0;
  }

  return gpio_button_enabled_flags[index];
}

/**
 * @brief Check if a button was pressed once (one-shot).
 *
 * @param index Button index
 * @retval 1 if pressed once, 0 otherwise
 */
inline uint8_t Gpio_Button_Is_Pressed_Once(uint8_t index) {
  if (!GPIO_BUTTON_INDEX_IS_VALID(index)) {
#ifdef GPIO_BUTTON_DEBUG
    printf("Invalid Button Index: %d\r\n", index);
#endif
    return 0;
  }

  return gpio_button_pressed_flags[index];
}
