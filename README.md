# STM32 Multimeter Project

This project implements a capacitor measurement system on the STM32F1 series MCU.  
It uses PWM generation, PWM input capture, ADC sampling, and LCD display via I2C.

## Features
- PWM output with frequency & duty control (<1% error)
- PWM input capture for frequency and duty measurement
- Capacitor value estimation using timing & ADC methods
- LCD display with custom icons (Ω, ↑, ↓)
- Modular drivers: GPIO, PWM, ADC, I2C, LCD

## Tools
- STM32 Standard Peripheral Library
- Keil uVision
- Hardware: STM32F103C8T6, I2C LCD, basic passive components

## Author
Nguyen Ngoc Hai – April 2025
