#ifndef PWM_CAPTURE_DRIVER_H
#define PWM_CAPTURE_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define PWM_CAPTURE_LOW_FREQ_THRESHOLD_HZ  200U
#define PWM_CAPTURE_MID_FREQ_THRESHOLD_HZ 4000U

typedef struct
{
  uint8_t fresh;
  uint32_t period_ticks;
  uint32_t high_ticks;
  uint32_t timer_clock_hz;
  uint32_t last_capture_time_ms;
} PwmCaptureSample_t;

void PwmCaptureDriver_Init(void);
void PwmCaptureDriver_Enable(uint32_t expected_frequency_hz);
void PwmCaptureDriver_Disable(void);
uint8_t PwmCaptureDriver_Read(PwmCaptureSample_t* sample);

#ifdef __cplusplus
}
#endif

#endif /* PWM_CAPTURE_DRIVER_H */
