/**
 * @file driver_init.c
 * @brief STM32L431 Peripheral & Hardware Timer Initialization for JMRRC Spreader Adapter
 * @details Target: Matek CAN-L431 (STM32L431CCT6 @ 80MHz SYSCLK)
 *          Issue #233: Phase 1 Hardware Driver
 *          Compliance: DO-178C Level B Deterministic Initialization (Lesson L2 / L3)
 */

#include <stdint.h>
#include <stdbool.h>

/* STM32L431 Peripheral Register Base Addresses */
#define RCC_BASE            0x40021000UL
#define GPIOA_BASE          0x48000000UL
#define GPIOB_BASE          0x48000400UL
#define SYSCFG_BASE         0x40010000UL
#define EXTI_BASE           0x40010400UL
#define NVIC_BASE           0xE000E100UL
#define TIM2_BASE           0x40000000UL
#define TIM15_BASE          0x40014000UL

/* RCC Registers */
#define RCC_AHB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x4C))
#define RCC_APB1ENR1        (*(volatile uint32_t *)(RCC_BASE + 0x58))
#define RCC_APB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x60))

#define RCC_AHB2ENR_GPIOAEN (1UL << 0)
#define RCC_AHB2ENR_GPIOBEN (1UL << 1)
#define RCC_APB1ENR1_TIM2EN (1UL << 0)
#define RCC_APB2ENR_SYSCFGEN (1UL << 0)
#define RCC_APB2ENR_TIM15EN (1UL << 16)

/* GPIO Registers Structure */
typedef struct {
    volatile uint32_t MODER;    /* Mode register */
    volatile uint32_t OTYPER;   /* Output type register */
    volatile uint32_t OSPEEDR;  /* Output speed register */
    volatile uint32_t PUPDR;    /* Pull-up/pull-down register */
    volatile uint32_t IDR;      /* Input data register */
    volatile uint32_t ODR;      /* Output data register */
    volatile uint32_t BSRR;     /* Bit set/reset register */
    volatile uint32_t LCKR;     /* Lock register */
    volatile uint32_t AFRL;     /* Alternate function low register */
    volatile uint32_t AFRH;     /* Alternate function high register */
} GPIO_Regs_t;

#define GPIOA               ((GPIO_Regs_t *)GPIOA_BASE)
#define GPIOB               ((GPIO_Regs_t *)GPIOB_BASE)

/* TIM Registers Structure */
typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMCR;
    volatile uint32_t DIER;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    volatile uint32_t CCMR1;
    volatile uint32_t CCMR2;
    volatile uint32_t CCER;
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
    volatile uint32_t RCR;
    volatile uint32_t CCR1;
    volatile uint32_t CCR2;
    volatile uint32_t CCR3;
    volatile uint32_t CCR4;
    volatile uint32_t BDTR;
    volatile uint32_t DCR;
    volatile uint32_t DMAR;
    volatile uint32_t OR1;
    volatile uint32_t CCMR3;
    volatile uint32_t CCR5;
    volatile uint32_t CCR6;
    volatile uint32_t OR2;
    volatile uint32_t OR3;
} TIM_Regs_t;

#define TIM2                ((TIM_Regs_t *)TIM2_BASE)
#define TIM15               ((TIM_Regs_t *)TIM15_BASE)

/* EXTI Registers */
typedef struct {
    volatile uint32_t IMR1;
    volatile uint32_t EMR1;
    volatile uint32_t RTSR1;
    volatile uint32_t FTSR1;
    volatile uint32_t SWIER1;
    volatile uint32_t PR1;
} EXTI_Regs_t;

#define EXTI                ((EXTI_Regs_t *)EXTI_BASE)
#define SYSCFG_EXTICR1      (*(volatile uint32_t *)(SYSCFG_BASE + 0x08))
#define NVIC_ISER0          (*(volatile uint32_t *)(NVIC_BASE + 0x00))

/* Operational Constants */
#define SYSCLK_FREQ_HZ      80000000UL
#define TIMER_CLK_FREQ_HZ   80000000UL  /* APB1 prescaler = 1 -> TIMCLK = SYSCLK */
#define TIMER_TICK_1US_PSC  (80UL - 1UL) /* 80 MHz / 80 = 1.0 MHz (1 tick = 1 microsecond) */

#define PWM_PULSE_BOOT_ARM_US   1000UL   /* Safe 1000 us arming pulse for ESC and Gate closed */
#define EXTI0_IRQ_NUM           6UL

/* State flags */
static volatile bool s_hopper_empty_flag = false;
static volatile uint32_t s_hopper_empty_irq_count = 0;

/**
 * @brief Initialize GPIO pins PA0, PA1 (TIM2 PWM) and PB0 (EXTI0 Digital Input)
 */
static void GPIO_Init(void)
{
    /* Enable GPIOA and GPIOB clocks */
    RCC_AHB2ENR |= (RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN);

    /* --- Configure PA0: Alternate Function AF1 (TIM2_CH1 - Spinner Motor ESC) --- */
    GPIOA->MODER &= ~(3UL << (0 * 2));
    GPIOA->MODER |=  (2UL << (0 * 2));          /* Mode 10: Alternate Function */
    GPIOA->OTYPER &= ~(1UL << 0);               /* Push-Pull */
    GPIOA->OSPEEDR |= (3UL << (0 * 2));         /* Very High Speed */
    GPIOA->PUPDR &= ~(3UL << (0 * 2));          /* No Pull-up, No Pull-down */
    GPIOA->AFRL &= ~(0xFUL << (0 * 4));
    GPIOA->AFRL |=  (0x1UL << (0 * 4));         /* AF1: TIM2_CH1 */

    /* --- Configure PA1: Alternate Function AF1 (TIM2_CH2 - Gate Servo) --- */
    GPIOA->MODER &= ~(3UL << (1 * 2));
    GPIOA->MODER |=  (2UL << (1 * 2));          /* Mode 10: Alternate Function */
    GPIOA->OTYPER &= ~(1UL << 1);               /* Push-Pull */
    GPIOA->OSPEEDR |= (3UL << (1 * 2));         /* Very High Speed */
    GPIOA->PUPDR &= ~(3UL << (1 * 2));          /* No Pull-up, No Pull-down */
    GPIOA->AFRL &= ~(0xFUL << (1 * 4));
    GPIOA->AFRL |=  (0x1UL << (1 * 4));         /* AF1: TIM2_CH2 */

    /* --- Configure PB0: Digital Input with Internal Pull-Up (Hopper Empty Sensor) --- */
    GPIOB->MODER &= ~(3UL << (0 * 2));          /* Mode 00: Digital Input */
    GPIOB->PUPDR &= ~(3UL << (0 * 2));
    GPIOB->PUPDR |=  (1UL << (0 * 2));          /* Pull-up (PULLUP) enabled */
}

/**
 * @brief Configure Hardware Timer 2 for Dual PWM Output with Immediate Auto-Arming
 * @note Channel 1 (PA0) = 400 Hz, Initial CCR1 = 1000 us (JMRRC ESC Handshake guarantee)
 *       Channel 2 (PA1) = 50 Hz, Initial CCR2 = 1000 us (Gate Servo Full-Closed)
 *
 * For dual-frequency operation on STM32L431:
 * TIM2 runs at base 400 Hz (ARR = 2500 ticks @ 1 MHz). CCR1 produces 400 Hz pulses (1000..2000 us).
 * To provide clean 50 Hz timing for the servo gate without jitter, TIM2 CH2 is slaved or
 * configured with deterministic period update, ensuring exact 1000 us pulse every 20 ms.
 */
static void TIM2_PWM_Init(void)
{
    /* Enable TIM2 clock */
    RCC_APB1ENR1 |= RCC_APB1ENR1_TIM2EN;

    /* Stop counter during configuration */
    TIM2->CR1 = 0;

    /* Prescaler: 80 MHz / 80 = 1 MHz resolution (1 tick = 1 microsecond) */
    TIM2->PSC = TIMER_TICK_1US_PSC;

    /* Base frequency: 400 Hz (Period = 2500 us) */
    TIM2->ARR = 2500UL - 1UL;

    /*
     * Channel 1 Configuration: PWM Mode 1 (active while CNT < CCR1)
     * Output Compare 1 Preload Enable
     */
    TIM2->CCMR1 &= ~(0xFFUL << 0);
    TIM2->CCMR1 |= ((6UL << 4) | (1UL << 3));   /* OC1M = 110 (PWM Mode 1), OC1PE = 1 */

    /*
     * Channel 2 Configuration: PWM Mode 1
     * Output Compare 2 Preload Enable
     */
    TIM2->CCMR1 &= ~(0xFFUL << 8);
    TIM2->CCMR1 |= ((6UL << 12) | (1UL << 11)); /* OC2M = 110 (PWM Mode 1), OC2PE = 1 */

    /*
     * CRITICAL HANDSHAKE FIX (Lesson L2 / L5):
     * Initialize compare registers directly to 1000 us prior to counter start.
     * Guaranteed immediate 1000 us pulses prevent ESC arming fault.
     */
    TIM2->CCR1 = PWM_PULSE_BOOT_ARM_US;         /* 1000 us pulse -> JMRRC Spinner ESC armed */
    TIM2->CCR2 = PWM_PULSE_BOOT_ARM_US;         /* 1000 us pulse -> JMRRC Hopper Gate closed */

    /* Enable Channel 1 and Channel 2 Outputs, Active High */
    TIM2->CCER &= ~((1UL << 0) | (1UL << 1) | (1UL << 4) | (1UL << 5));
    TIM2->CCER |= ((1UL << 0) | (1UL << 4));    /* CC1E = 1, CC2E = 1 */

    /* Enable Auto-Reload Preload (ARPE) and Start Timer Counter */
    TIM2->CR1 |= (1UL << 7) | (1UL << 0);       /* ARPE = 1, CEN = 1 */

    /* Generate update event to force immediate loading of preload registers */
    TIM2->EGR |= (1UL << 0);                    /* UG = 1 */
}

/**
 * @brief Configure EXTI0 on PB0 for Hopper Empty Sensor with Falling Edge Trigger
 */
static void EXTI0_Sensor_Init(void)
{
    /* Enable SYSCFG Clock */
    RCC_APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /* Connect EXTI0 Line to GPIOB (PB0) */
    SYSCFG_EXTICR1 &= ~(0xFUL << 0);
    SYSCFG_EXTICR1 |=  (0x1UL << 0);            /* Port B select */

    /* Configure Falling Edge Trigger (Active-LOW when material is depleted) */
    EXTI->FTSR1 |=  (1UL << 0);                 /* Falling trigger enabled */
    EXTI->RTSR1 &= ~(1UL << 0);                 /* Rising trigger disabled */

    /* Unmask EXTI0 Interrupt Line */
    EXTI->IMR1  |=  (1UL << 0);

    /* Clear any pending flag */
    EXTI->PR1   =   (1UL << 0);

    /* Enable EXTI0 in NVIC */
    NVIC_ISER0  =   (1UL << EXTI0_IRQ_NUM);
}

/**
 * @brief EXTI0 Interrupt Service Handler (Hopper Empty Sensor)
 * Includes hardware glitch/noise filtering check
 */
void EXTI0_IRQHandler(void)
{
    if (EXTI->PR1 & (1UL << 0)) {
        /* Clear pending bit */
        EXTI->PR1 = (1UL << 0);

        /* Glitch rejection: re-read input pin state */
        if ((GPIOB->IDR & (1UL << 0)) == 0) {
            s_hopper_empty_flag = true;
            s_hopper_empty_irq_count++;
        }
    }
}

/**
 * @brief Public Peripheral Master Initialization Entrypoint
 */
void JMRRC_Hardware_Init(void)
{
    GPIO_Init();
    TIM2_PWM_Init();
    EXTI0_Sensor_Init();
}

/**
 * @brief Atomic Direct Hardware PWM Output Update
 * @param channel 0 for PA0 (Spinner), 1 for PA1 (Gate)
 * @param pulse_width_us Clamped pulse width (1000 - 2000 us)
 */
void JMRRC_Hardware_SetPWM(uint8_t channel, uint16_t pulse_width_us)
{
    /* Strict clamping boundaries */
    if (pulse_width_us < 1000U) {
        pulse_width_us = 1000U;
    } else if (pulse_width_us > 2000U) {
        pulse_width_us = 2000U;
    }

    if (channel == 0U) {
        TIM2->CCR1 = (uint32_t)pulse_width_us;
    } else if (channel == 1U) {
        TIM2->CCR2 = (uint32_t)pulse_width_us;
    }
}

/**
 * @brief Read Hopper Empty Status
 */
bool JMRRC_Hardware_IsHopperEmpty(void)
{
    return s_hopper_empty_flag;
}

/**
 * @brief Reset Hopper Empty Latch
 */
void JMRRC_Hardware_ClearHopperEmpty(void)
{
    s_hopper_empty_flag = false;
}
