# STM32 Multimeter Project

This project is a multimeter system based on the STM32F1 series MCU.
It uses PWM generation, PWM input capture, ADC sampling, and LCD display via I2C.

## Features

* PWM output with frequency and duty cycle control (<1% error)
* PWM input capture for frequency and duty cycle measurement
* Capacitor and resistor value estimation using timing and ADC methods
* LCD display with custom icons (Ω, ↑, ↓)
* Modular drivers: GPIO, PWM, ADC, I2C, SYSTICK, USART, FLASH

## Hardware

* STM32F103C8T6
* LCD1602
* I2C LCD module
* Push buttons
* Resistors and capacitors for measurement testing
* ST-Link programmer/debugger
* External signal source for PWM testing

## Software / Tools

* STM32 Standard Peripheral Library
* Keil uVision
* STM32CubeProgrammer or ST-Link Utility
* GitHub for version control

## Project Structure

```txt
Multimeter_Project/
├── MDK/
│   └── Keil project and build configuration files
├── USER/
│   └── Main application source code and user modules
└── README.md
```

## Measurement Methods

### Frequency Measurement

The input PWM frequency is measured using the timer input capture feature of the STM32F1 MCU.
The timer captures signal edges and calculates the signal period, then the frequency is obtained from the timer clock and captured period value.

### Duty Cycle Measurement

The duty cycle is calculated from the high-level time and the total signal period.
PWM input capture is used to determine both the pulse width and the full period of the input signal.

### Capacitance Measurement

Capacitance is estimated using an RC timing method.
The capacitor charging or discharging time is measured, then the capacitance value is calculated based on the known resistance and timing result.

### Resistance Measurement

Resistance is estimated using an ADC-based voltage measurement method.
The unknown resistor is measured through a voltage divider circuit, and the resistance value is calculated from the ADC reading.

## Frequency Measurement Results

| Function                  | Test Parameter | Measured Result | Estimated Error |
| ------------------------- | -------------: | --------------: | --------------: |
| PWM frequency measurement |         100 Hz |     99 – 101 Hz |            < 1% |
| PWM frequency measurement |         500 Hz |    496 – 504 Hz |            < 1% |
| PWM frequency measurement |          1 kHz | 0.99 – 1.01 kHz |            < 1% |
| PWM frequency measurement |          5 kHz | 4.95 – 5.05 kHz |            < 1% |
| PWM frequency measurement |         10 kHz |  9.9 – 10.1 kHz |            < 1% |
| PWM frequency measurement |         20 kHz | 19.8 – 20.2 kHz |            < 1% |
| PWM frequency measurement |         50 kHz | 49.5 – 50.5 kHz |            < 1% |
| PWM frequency measurement |        100 kHz |    99 – 101 kHz |            < 1% |

## Duty Cycle Measurement Results

| Function                   | Test Parameter | Measured Result | Estimated Error |
| -------------------------- | -------------: | --------------: | --------------: |
| PWM duty cycle measurement |            10% |     9.9 – 10.1% |            < 1% |
| PWM duty cycle measurement |            20% |    19.8 – 20.2% |            < 1% |
| PWM duty cycle measurement |            30% |    29.7 – 30.3% |            < 1% |
| PWM duty cycle measurement |            40% |    39.6 – 40.4% |            < 1% |
| PWM duty cycle measurement |            50% |    49.5 – 50.5% |            < 1% |
| PWM duty cycle measurement |            60% |    59.4 – 60.6% |            < 1% |
| PWM duty cycle measurement |            70% |    69.3 – 70.7% |            < 1% |
| PWM duty cycle measurement |            80% |    79.2 – 80.8% |            < 1% |
| PWM duty cycle measurement |            90% |    89.1 – 90.9% |            < 1% |

## Capacitance Measurement Results

| Function                | Test Parameter | Measured Result | Estimated Error |
| ----------------------- | -------------: | --------------: | --------------: |
| Capacitance measurement |         100 nF |     90 – 115 nF |            ~15% |
| Capacitance measurement |         220 nF |    200 – 250 nF |            ~14% |
| Capacitance measurement |         470 nF |    430 – 520 nF |            ~11% |
| Capacitance measurement |           1 µF |    0.9 – 1.1 µF |            ~10% |
| Capacitance measurement |         2.2 µF |   2.0 – 2.45 µF |            ~11% |
| Capacitance measurement |         4.7 µF |    4.3 – 5.2 µF |            ~11% |
| Capacitance measurement |          10 µF |   9.5 – 10.8 µF |             ~8% |
| Capacitance measurement |          22 µF |      20 – 25 µF |            ~13% |
| Capacitance measurement |          47 µF |      43 – 52 µF |            ~11% |
| Capacitance measurement |         100 µF |     95 – 110 µF |            ~10% |
| Capacitance measurement |         220 µF |    205 – 245 µF |            ~11% |
| Capacitance measurement |         470 µF |    440 – 520 µF |            ~11% |
| Capacitance measurement |        1000 µF |   930 – 1120 µF |            ~12% |

## Resistance Measurement Results

| Function               | Test Parameter | Measured Result | Estimated Error |
| ---------------------- | -------------: | --------------: | --------------: |
| Resistance measurement |          100 Ω |      96 – 104 Ω |             ~4% |
| Resistance measurement |          220 Ω |     213 – 228 Ω |             ~3% |
| Resistance measurement |          330 Ω |     320 – 342 Ω |             ~3% |
| Resistance measurement |          470 Ω |     455 – 488 Ω |             ~4% |
| Resistance measurement |           1 kΩ |  0.98 – 1.02 kΩ |             ~2% |
| Resistance measurement |         2.2 kΩ |  2.15 – 2.25 kΩ |             ~2% |
| Resistance measurement |         4.7 kΩ |  4.58 – 4.82 kΩ |             ~3% |
| Resistance measurement |          10 kΩ |   9.8 – 10.3 kΩ |             ~3% |
| Resistance measurement |          22 kΩ |  21.3 – 22.8 kΩ |             ~4% |
| Resistance measurement |          47 kΩ |  45.5 – 49.0 kΩ |             ~4% |
| Resistance measurement |         100 kΩ |     96 – 105 kΩ |             ~5% |
| Resistance measurement |         220 kΩ |    210 – 235 kΩ |             ~7% |
| Resistance measurement |         470 kΩ |    445 – 510 kΩ |             ~8% |
| Resistance measurement |           1 MΩ |  0.92 – 1.10 MΩ |            ~10% |

## How to Build

1. Clone this repository.

```bash
git clone https://github.com/haikevins/Multimeter_Project.git
```

2. Open the Keil project file inside the `MDK` folder.
3. Select the target device as `STM32F103C8T6`.
4. Build the project in Keil uVision.
5. Make sure the build process finishes without errors.

## How to Flash

1. Connect the STM32F103C8T6 board to the computer using ST-Link.
2. Open Keil uVision, STM32CubeProgrammer, or ST-Link Utility.
3. Load the generated firmware file.
4. Flash the firmware to the MCU.
5. Reset the board and check the LCD display.

## Limitations

* Capacitance measurement accuracy depends on the RC timing circuit and the tolerance of the reference resistor.
* Resistance measurement accuracy depends on ADC stability, voltage reference accuracy, and resistor tolerance.
* High-frequency PWM measurement may be affected by timer configuration and signal noise.
* The measured values are suitable for educational and experimental purposes.

## Future Improvements

* Add auto-ranging for resistance and capacitance measurement.
* Improve ADC filtering for more stable readings.
* Add calibration support for resistance and capacitance modes.
* Add UART output for logging measurement data.
* Add more detailed hardware documentation and circuit diagrams.

## Author

Nguyen Ngoc Hai – April 2025

## License

This project is intended for learning, research, and embedded system practice.
