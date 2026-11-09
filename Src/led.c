#include "led.h"

#include "stm32f7xx_hal.h"

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} led_pin_t;

/* Pins are listed in physical order. */
static const led_pin_t leds[] = {
    { GPIOB, GPIO_PIN_11 },
    { GPIOB, GPIO_PIN_10 },
    { GPIOE, GPIO_PIN_15 },
    { GPIOE, GPIO_PIN_14 },
    { GPIOE, GPIO_PIN_12 },
    { GPIOE, GPIO_PIN_10 },
    { GPIOE, GPIO_PIN_0  },
    { GPIOE, GPIO_PIN_7  },
    { GPIOE, GPIO_PIN_8  },
    { GPIOG, GPIO_PIN_9  },
    { GPIOG, GPIO_PIN_14 },
    { GPIOF, GPIO_PIN_15 },
    { GPIOE, GPIO_PIN_13 },
    { GPIOF, GPIO_PIN_14 },
    { GPIOE, GPIO_PIN_11 },
    { GPIOE, GPIO_PIN_9  },
    { GPIOF, GPIO_PIN_13 },
    { GPIOB, GPIO_PIN_1  },
    { GPIOC, GPIO_PIN_2  },
    { GPIOF, GPIO_PIN_4  }
};

#define LED_COUNT           ((uint32_t)(sizeof(leds) / sizeof(leds[0])))
#define LED_ACTIVE_COUNT    4U
#define LED_HALF_ACTIVE     (LED_ACTIVE_COUNT / 2U)

/* Range of the angle that is mapped across the strip. */
#define LED_ANGLE_MIN_DEG   (-20.0f)
#define LED_ANGLE_MAX_DEG   (20.0f)
#define LED_ANGLE_SPAN_DEG  (LED_ANGLE_MAX_DEG - LED_ANGLE_MIN_DEG)

#define LED_ON_STATE        GPIO_PIN_SET
#define LED_OFF_STATE       GPIO_PIN_RESET

static uint32_t angle_to_center_led(float angle_deg)
{
    if (angle_deg < LED_ANGLE_MIN_DEG) {
        angle_deg = LED_ANGLE_MIN_DEG;
    } else if (angle_deg > LED_ANGLE_MAX_DEG) {
        angle_deg = LED_ANGLE_MAX_DEG;
    }

    /* Map the clamped angle onto the LED indices, rounded to nearest. */
    return (uint32_t)(((angle_deg - LED_ANGLE_MIN_DEG) / LED_ANGLE_SPAN_DEG) *
                      (float)(LED_COUNT - 1U) + 0.5f);
}

static uint32_t first_active_led(uint32_t center)
{
    uint32_t last = LED_COUNT - LED_ACTIVE_COUNT;
    uint32_t first = (center >= LED_HALF_ACTIVE)
                     ? (center - LED_HALF_ACTIVE) : 0U;

    /* Keep the whole block on the strip near either end. */
    return (first > last) ? last : first;
}

void led_show_angle(float angle_deg)
{
    uint32_t first = first_active_led(angle_to_center_led(angle_deg));
    uint32_t last = first + LED_ACTIVE_COUNT;

    for (uint32_t i = 0U; i < LED_COUNT; i++) {
        GPIO_PinState state = ((i >= first) && (i < last))
                              ? LED_ON_STATE : LED_OFF_STATE;

        HAL_GPIO_WritePin(leds[i].port, leds[i].pin, state);
    }
}
