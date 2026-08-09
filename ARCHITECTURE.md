# Mini Multimeter - Layered Architecture

## 1. Layer dependency rule

Dependencies only go downward:

```text
Application
    |
    v
Services
    |
    v
Drivers
    |
    v
Infrastructure
    |
    v
Platform
    |
    v
STM32F10x SPL / CMSIS / Hardware
```

An upper layer may call a lower layer directly when it is a device-facing UI/bootstrap concern, but a lower layer must never include or call an upper layer.

## 2. Project structure

```text
Application/
  main.c
  app_controller.c/.h
  ui_controller.c/.h
  ui_view.c/.h
  ui_formatter.c/.h

Services/
  config_service.c/.h
  measurement_service.c/.h
  signal_generator_service.c/.h

Drivers/
  adc_driver.c/.h
  button_driver.c/.h
  capacitor_charge_driver.c/.h
  flash_storage_driver.c/.h
  lcd1602_driver.c/.h
  pwm_capture_driver.c/.h
  pwm_output_driver.c/.h

Infrastructure/
  debug_logger.c/.h
  error_manager.c/.h

Platform/
  platform_init.c/.h
  system_time.c/.h
  uart_port.c/.h

MDK/
  Keil project + CMSIS/RTE/vendor files
```

## 3. Responsibilities

### Application
- `main.c`: only starts the application and executes the super-loop.
- `app_controller`: boot order and top-level process loop.
- `ui_controller`: menu FSM, navigation, cursor memory, transient UI states, and conversion of button events into use-case calls.
- `ui_view`: LCD1602 presentation only: list rendering, titles, status messages, editors, and feedback screens.
- `ui_formatter`: pure value/string formatting for resistance, capacitance, frequency, duty, and centered editor text.

### Services
- `config_service`: owns runtime frequency/duty/step settings and requests persistence.
- `measurement_service`: owns resistor/capacitor/frequency/duty measurement workflows, measurement-mode transitions, and UI-facing measurement status (`MEASURING`, `CHARGING`, `NO_SIGNAL`, `NO_COMPONENT`, `ERROR`, etc.).
- `signal_generator_service`: clamps requested PWM settings and calculates timer prescaler/ARR/CCR values.

### Drivers
- `adc_driver`: interrupt-driven 16-sample ADC batches and LPF.
- `button_driver`: GPIO button sampling, 20 ms debounce, and event queue.
- `capacitor_charge_driver`: PA5 charge/discharge hardware control.
- `flash_storage_driver`: versioned + CRC-protected Flash record at the reserved final page.
- `lcd1602_driver`: LCD1602/PCF8574 over I2C2 with timeout/error handling.
- `pwm_capture_driver`: TIM1/TIM3/TIM4 input capture and active-range selection.
- `pwm_output_driver`: TIM2 CH2 hardware initialization/enable/disable and timer-register application.

### Infrastructure
- `debug_logger`: timestamped UART log levels.
- `error_manager`: in-RAM error history/coalescing/flush.

### Platform
- `platform_init`: wrapper around MCU `SystemInit()`.
- `system_time`: SysTick 1 ms timebase.
- `uart_port`: low-level USART1 transport used by diagnostics.

## 4. Runtime flow (unchanged)

```text
Reset
  -> AppController_Init()
      -> Platform_InitSystem()
      -> ErrorManager_Init()
      -> ConfigService_Init() -> FlashStorage_Load()
      -> SystemTime_Init()
      -> ButtonDriver_Init()
      -> UartPort_Init() / DebugLogger
      -> MeasurementService_Init()
      -> SignalGeneratorService_Init()
      -> Lcd1602Driver_Init()
      -> UiController_Init()

while (1)
  -> AppController_RunOnce()
      -> ButtonDriver_Process()
      -> consume ButtonEvent_t
      -> UiController_HandleEvent()
      -> UiController_Update()
```

## 5. Measurement flow

`ui_controller` no longer manipulates ADC/TIM or LCD1602 directly. It selects a measurement mode:

```text
UiController
  -> MeasurementService_SetMode()
       -> resistor: ADC driver
       -> capacitor: ADC + capacitor charge driver
       -> freq/duty: PWM output + PWM capture drivers
```

The service also exposes `MeasurementService_GetStatus()`. The UI never infers state from a numeric result such as `0.0`; it renders `Measuring...`, `Press Charge`, `Discharging...`, `Charging...`, `No Signal`, or `Error` from the service state.

The original All Mode resource-sharing behavior is retained:
- Resistor -> Capacitor keeps ADC enabled and cancels only the old conversion batch.
- Frequency -> Duty keeps PWM capture/output enabled.

## 6. Signal generation flow

```text
UiController
  -> ConfigService
  -> SignalGeneratorService_Apply()
       -> calculate PSC/ARR/CCR
       -> PwmOutputDriver_ApplyTimer()
```

The Service layer contains the frequency calculation. The Driver layer alone writes TIM2 registers.

## 7. Non-blocking boot splash

The boot title is a transient UI state, not a presentation delay:

```text
UiController_Init()
  -> MENU_SPLASH
       -> render project title
       -> store splashStartTick

AppController_RunOnce() continues normally
  -> ButtonDriver_Process()
  -> UiController_Update()
       -> after 2000 ms
       -> MENU_MAIN
```

Button events are ignored while the splash is visible, but the application super-loop is not blocked. There is no `SystemTime_DelayMs()` in the Application layer.

## 8. Non-blocking save feedback

Configuration confirmation is represented by a transient UI state rather than a delay:

```text
SELECT
  -> ConfigService_Save()
  -> MENU_SAVE_FEEDBACK
       -> Saved! / Save Error
       -> wait 800 ms using SystemTime_GetTick()
       -> next menu (or return to the same editor on save failure)
```

Button events are ignored while the short feedback state is active. The super-loop continues to run; no `DelayMs()` is used for save feedback.

## 9. Per-menu cursor memory

Each list-style UI state owns its own cursor slot:

```text
Main        -> remembers Measure / Transmit
Measure     -> remembers Single / All
Single Mode -> remembers Resistor / Capacitor / Frequency / Duty
Transmit    -> remembers Frequency / Duty / Setting
```

Entering a child state does not overwrite the parent cursor. Returning from a measurement or editor therefore highlights the item that launched it. `Back` is treated as a navigation command rather than a resume position; selecting it restores the menu's last non-Back item before leaving.

## 10. UI responsibility split

The Application UI is split by responsibility without changing the menu flow:

```text
ButtonEvent_t
  -> UiController
       -> decides state/navigation/action
       -> UiView
            -> requests formatting from UiFormatter
            -> writes through Lcd1602Driver
```

Rules:
- `ui_controller.c` does not include or call `lcd1602_driver` and does not format display strings.
- `ui_view.c` owns all LCD1602 rendering details and custom scroll-character placement.
- `ui_formatter.c` has no hardware/service dependency; it only converts values to display text.

This keeps the existing FSM, 300 ms smart refresh, non-blocking splash/save feedback, measurement statuses, and per-menu cursor memory unchanged.

## 11. Persistent settings flow

```text
UiController
  -> ConfigService_Save()
       -> FlashStorage_Save()
            -> version + CRC + verify

Reset
  -> ConfigService_Init()
       -> FlashStorage_Load()
```

The final 1 KB Flash page (`0x0800FC00..0x0800FFFF`) remains reserved from application code.

## 12. Old -> new file mapping

| Previous file | New file |
|---|---|
| `app.c` | `Application/app_controller.c` |
| `menu.c` | `Application/ui_controller.c` + `ui_view.c` + `ui_formatter.c` |
| `app_config.c` | `Services/config_service.c` |
| `measure.c` | `Services/measurement_service.c` |
| `transmit.c` | `Services/signal_generator_service.c` |
| `adc.c` | `Drivers/adc_driver.c` |
| `gpio.c` | `Drivers/button_driver.c` |
| charge GPIO inside `measure.c` | `Drivers/capacitor_charge_driver.c` |
| `i2c2_lcd.c` | `Drivers/lcd1602_driver.c` |
| `pwm_input.c` | `Drivers/pwm_capture_driver.c` |
| `pwm_output.c` | `Drivers/pwm_output_driver.c` |
| `flash.c` | `Drivers/flash_storage_driver.c` |
| `debug_log.c` | `Infrastructure/debug_logger.c` |
| `error_manager.c` | `Infrastructure/error_manager.c` |
| `systick.c` | `Platform/system_time.c` |
| `usart1.c` | `Platform/uart_port.c` |


## Component-presence validation

- Resistance UI refresh is intentionally held to 1000 ms while ADC acquisition continues at full speed.
- An open resistor socket is detected near ADC full-scale with hysteresis and three-batch confirmation. The threshold preserves the documented 1 Mohm range; very large resistances above the supported range may intentionally be treated as open.
- Capacitor presence is validated during the timed low-to-high charging phase. An immediate first-batch threshold crossing is treated as an empty socket / capacitance below the documented 100 nF range.
- `NO_COMPONENT` is a normal measurement status, not an Error Manager fault.
