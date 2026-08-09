#include "pwm_capture_driver.h"
#include "stm32f10x.h"
#include <stddef.h>
#include "debug_logger.h"
#include "system_time.h"


typedef enum {
  PWM_CHANNEL_1 = 0,
  PWM_CHANNEL_3,
  PWM_CHANNEL_4,
  PWM_CHANNEL_NUM
} PWM_Channel_t;

typedef struct
{
  volatile uint8_t ready_flag;
  volatile uint32_t period;
  volatile uint32_t high_time;
  volatile uint32_t last_capture_time_ms;
  uint32_t timer_clock_hz;
} PwmCaptureDriver_Capture;

#define PWM_CHANNEL_INVALID ((uint8_t)0xFFU)

/** 
  * @brief Global PWM input capture structures.
  * @details Each entry stores captured period, high time, and timestamp for timeout handling.
  */
static PwmCaptureDriver_Capture pwm_inputs[PWM_CHANNEL_NUM] = {
  [PWM_CHANNEL_1] = { .timer_clock_hz = 72e6 / 1.0f },     // TIM1
  [PWM_CHANNEL_3] = { .timer_clock_hz = 72e6 / 8.0f },     // TIM3
  [PWM_CHANNEL_4] = { .timer_clock_hz = 72e6 / 1099.0f },  // TIM4
};

static uint8_t pwm_active_channel = PWM_CHANNEL_INVALID;

/**
  * @brief Initialize one timer channel for PWM input capture.
  * @param TIMx        Timer instance (TIM1, TIM3, TIM4).
  * @param GPIOx       GPIO port used by timer channel.
  * @param GPIO_Pin    Pin number for input capture.
  * @param rcc_apb_tim RCC clock mask for timer.
  * @param irq_channel NVIC IRQ channel of timer.
  * @param prescaler   Prescaler value to adjust frequency range.
  * @retval None
  */
static void Pwm_TIMx_Input_Init(
  TIM_TypeDef* TIMx,
  GPIO_TypeDef* GPIOx,

  uint16_t GPIO_Pin,
  uint16_t rcc_apb_tim,
  uint8_t irq_channel,
  uint16_t prescaler);

/**
  * @brief Disable all PWM input timers and clear pending capture state.
  */
static void PwmCaptureDriver_Disable_All(void);

/**
  * @brief Prepare one PWM input timer before enabling it.
  * @param TIMx Timer instance to prepare.
  * @param pwm  Capture state associated with TIMx.
  */
static void PwmCaptureDriver_Prepare(TIM_TypeDef* TIMx, PwmCaptureDriver_Capture* pwm);

/**
  * @brief Initialize all PWM input capture channels.
  * @note  Uses TIM1 (high freq), TIM3 (mid freq), and TIM4 (low freq).
  */
void PwmCaptureDriver_Init(void) {
  // TIM1 - PA8 - Prescaler = 0
  Pwm_TIMx_Input_Init(TIM1, GPIOA, GPIO_Pin_8, RCC_APB2Periph_TIM1, TIM1_CC_IRQn, 0);

  // TIM3 - PA6 - Prescaler = 7
  Pwm_TIMx_Input_Init(TIM3, GPIOA, GPIO_Pin_6, RCC_APB1Periph_TIM3, TIM3_IRQn, 7);

  // TIM4 - PB6 - Prescaler = 1098
  Pwm_TIMx_Input_Init(TIM4, GPIOB, GPIO_Pin_6, RCC_APB1Periph_TIM4, TIM4_IRQn, 1098);

  PwmCaptureDriver_Disable();
}

/**
  * @brief Enable PWM input on the appropriate timer depending on expected frequency.
  */
void PwmCaptureDriver_Enable(uint32_t expected_frequency_hz) {
  /* Always start from a known state: never allow two input timers to run together. */
  PwmCaptureDriver_Disable_All();

  if (expected_frequency_hz < PWM_CAPTURE_LOW_FREQ_THRESHOLD_HZ) {
    PwmCaptureDriver_Prepare(TIM4, &pwm_inputs[PWM_CHANNEL_4]);
    pwm_active_channel = PWM_CHANNEL_4;
    TIM_Cmd(TIM4, ENABLE);
  } else if (expected_frequency_hz < PWM_CAPTURE_MID_FREQ_THRESHOLD_HZ) {
    PwmCaptureDriver_Prepare(TIM3, &pwm_inputs[PWM_CHANNEL_3]);
    pwm_active_channel = PWM_CHANNEL_3;
    TIM_Cmd(TIM3, ENABLE);
  } else {
    PwmCaptureDriver_Prepare(TIM1, &pwm_inputs[PWM_CHANNEL_1]);
    pwm_active_channel = PWM_CHANNEL_1;
    TIM_Cmd(TIM1, ENABLE);
  }
  DebugLogger_Log(DEBUG_LEVEL_TRACE, "PWM_IN", "Enable");
}

/**
  * @brief Disable all PWM input timers.
  * @note  Disabling all timers avoids leaving a previously selected frequency
  *        range active when the requested frequency range changes.
  */
void PwmCaptureDriver_Disable(void) {
  PwmCaptureDriver_Disable_All();
  DebugLogger_Log(DEBUG_LEVEL_TRACE, "PWM_IN", "Disable");
}

uint8_t PwmCaptureDriver_Read(PwmCaptureSample_t* sample)
{
  PwmCaptureDriver_Capture* active;
  uint32_t primask;

  if ((sample == NULL) || (pwm_active_channel >= PWM_CHANNEL_NUM)) {
    return 0U;
  }

  active = &pwm_inputs[pwm_active_channel];

  primask = __get_PRIMASK();
  __disable_irq();
  sample->fresh = active->ready_flag;
  sample->period_ticks = active->period;
  sample->high_ticks = active->high_time;
  sample->timer_clock_hz = active->timer_clock_hz;
  sample->last_capture_time_ms = active->last_capture_time_ms;
  active->ready_flag = 0U;
  if (primask == 0U) {
    __enable_irq();
  }

  return 1U;
}

/* -------------------------------------------------------------------------- */
/*                          Static Helper Functions                           */
/* -------------------------------------------------------------------------- */

static void PwmCaptureDriver_Disable_All(void) {
  pwm_active_channel = PWM_CHANNEL_INVALID;
  TIM_Cmd(TIM1, DISABLE);
  TIM_Cmd(TIM3, DISABLE);
  TIM_Cmd(TIM4, DISABLE);

  TIM_SetCounter(TIM1, 0);
  TIM_SetCounter(TIM3, 0);
  TIM_SetCounter(TIM4, 0);

  TIM_ClearITPendingBit(TIM1, TIM_IT_CC1 | TIM_IT_Update);
  TIM_ClearITPendingBit(TIM3, TIM_IT_CC1 | TIM_IT_Update);
  TIM_ClearITPendingBit(TIM4, TIM_IT_CC1 | TIM_IT_Update);

  pwm_inputs[PWM_CHANNEL_1].ready_flag = 0;
  pwm_inputs[PWM_CHANNEL_3].ready_flag = 0;
  pwm_inputs[PWM_CHANNEL_4].ready_flag = 0;
}

static void PwmCaptureDriver_Prepare(TIM_TypeDef* TIMx, PwmCaptureDriver_Capture* pwm) {
  TIM_Cmd(TIMx, DISABLE);
  TIM_SetCounter(TIMx, 0);
  TIM_ClearITPendingBit(TIMx, TIM_IT_CC1 | TIM_IT_Update);

  pwm->ready_flag = 0;
  pwm->period = 0;
  pwm->high_time = 0;
  pwm->last_capture_time_ms = SystemTime_GetTick();
}

static void Pwm_TIMx_Input_Init(
  TIM_TypeDef* TIMx,
  GPIO_TypeDef* GPIOx,

  uint16_t GPIO_Pin,
  uint16_t rcc_apb_tim,
  uint8_t irq_channel,
  uint16_t prescaler) {
  /* Peripheral clock & GPIO configuration */
  if (TIMx == TIM1) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
  } else {
    RCC_APB1PeriphClockCmd(rcc_apb_tim, ENABLE);
  }

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA, ENABLE);
	
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOx, &GPIO_InitStructure);

  /* Timer base init */
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {
    .TIM_Prescaler = prescaler,
    .TIM_Period = 0xFFFF,
    .TIM_CounterMode = TIM_CounterMode_Up,
    .TIM_ClockDivision = TIM_CKD_DIV1,
    .TIM_RepetitionCounter = 0
  };
  TIM_TimeBaseInit(TIMx, &TIM_TimeBaseStructure);

  /* PWM input mode: Rising edge on CH1, Falling edge on CH2 */
  TIM_ICInitTypeDef IC_InitStructure = {
    .TIM_Channel = TIM_Channel_1,
    .TIM_ICPolarity = TIM_ICPolarity_Rising,
    .TIM_ICSelection = TIM_ICSelection_DirectTI,
    .TIM_ICPrescaler = TIM_ICPSC_DIV1,
    .TIM_ICFilter = 0
  };
  TIM_ICInit(TIMx, &IC_InitStructure);
	
  IC_InitStructure.TIM_Channel = TIM_Channel_2;
  IC_InitStructure.TIM_ICPolarity = TIM_ICPolarity_Falling;
  IC_InitStructure.TIM_ICSelection = TIM_ICSelection_IndirectTI;
  TIM_ICInit(TIMx, &IC_InitStructure);

  /* Configure timer in reset slave mode, triggered by TI1FP1 */
  TIM_SelectInputTrigger(TIMx, TIM_TS_TI1FP1);
  TIM_SelectSlaveMode(TIMx, TIM_SlaveMode_Reset);
  TIM_SelectMasterSlaveMode(TIMx, TIM_MasterSlaveMode_Enable);

  /* Enable CC and update interrupts */
  TIM_ITConfig(TIMx, TIM_IT_CC1 | TIM_IT_Update, ENABLE);

  /* TIM1 requires BDTR config to enable outputs */
  if (TIMx == TIM1) {
    TIM_BDTRInitTypeDef BDTR;
    TIM_BDTRStructInit(&BDTR);
    BDTR.TIM_OSSRState = TIM_OSSRState_Enable;
    BDTR.TIM_OSSIState = TIM_OSSIState_Enable;
    BDTR.TIM_LOCKLevel = TIM_LOCKLevel_OFF;
    BDTR.TIM_DeadTime = 0x00;
    BDTR.TIM_Break = TIM_Break_Disable;
    BDTR.TIM_BreakPolarity = TIM_BreakPolarity_High;
    BDTR.TIM_AutomaticOutput = TIM_AutomaticOutput_Enable;
    TIM_BDTRConfig(TIM1, &BDTR);

    TIM_CtrlPWMOutputs(TIM1, ENABLE);
  }

  /* Keep timer disabled after initialization. It is enabled only by
     PwmCaptureDriver_Enable() when that frequency range is actually used. */
  TIM_Cmd(TIMx, DISABLE);
  TIM_SetCounter(TIMx, 0);
  TIM_ClearITPendingBit(TIMx, TIM_IT_CC1 | TIM_IT_Update);

  /* NVIC configuration */
  NVIC_InitTypeDef NVIC_InitStructure = {
    .NVIC_IRQChannel = irq_channel,
    .NVIC_IRQChannelPreemptionPriority = 0,
    .NVIC_IRQChannelSubPriority = 1,
    .NVIC_IRQChannelCmd = ENABLE
  };
  NVIC_Init(&NVIC_InitStructure);
  NVIC_EnableIRQ((IRQn_Type)irq_channel);
}

/**
  * @brief Handle capture interrupt for a given timer and update PWM capture structure.
  * @param TIMx Pointer to timer peripheral.
  * @param pwm  Pointer to PWM input capture struct.
  */
static void Handle_Capture_Interrupt(TIM_TypeDef* TIMx, PwmCaptureDriver_Capture* pwm) {
  if (TIM_GetITStatus(TIMx, TIM_IT_CC1) != RESET) {
    TIM_ClearITPendingBit(TIMx, TIM_IT_CC1);

    uint32_t period = TIM_GetCapture1(TIMx);
    uint32_t high = TIM_GetCapture2(TIMx);

    if (high <= period && period != 0) {
      pwm->period = period;
      pwm->high_time = high;
      pwm->ready_flag = 1;
    }

    pwm->last_capture_time_ms = SystemTime_GetTick();
  }

  if (TIM_GetITStatus(TIMx, TIM_IT_Update) != RESET) {
    TIM_ClearITPendingBit(TIMx, TIM_IT_Update);
  }
}

/* -------------------------------------------------------------------------- */
/*                           Interrupt Handlers                               */
/* -------------------------------------------------------------------------- */

/**
  * @brief TIM1 Capture Compare interrupt handler.
  */
void TIM1_CC_IRQHandler(void) {
  Handle_Capture_Interrupt(TIM1, &pwm_inputs[PWM_CHANNEL_1]);
}

/**
  * @brief TIM3 global interrupt handler.
  */
void TIM3_IRQHandler(void) {
  Handle_Capture_Interrupt(TIM3, &pwm_inputs[PWM_CHANNEL_3]);
}

/**
  * @brief TIM4 global interrupt handler.
  */
void TIM4_IRQHandler(void) {
  Handle_Capture_Interrupt(TIM4, &pwm_inputs[PWM_CHANNEL_4]);
}

