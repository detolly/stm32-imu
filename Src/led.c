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

/* Range of the angle that is mapped across the strip. */
#define LED_ANGLE_MIN_DEG   (-20.0f)
#define LED_ANGLE_MAX_DEG   (20.0f)

#define LED_ON_STATE        GPIO_PIN_SET
#define LED_OFF_STATE       GPIO_PIN_RESET

void led_show_angle(float angle_deg)
{
    uint32_t center;
    uint32_t first;
    uint32_t i;

    if (angle_deg < LED_ANGLE_MIN_DEG) {
        angle_deg = LED_ANGLE_MIN_DEG;
    } else if (angle_deg > LED_ANGLE_MAX_DEG) {
        angle_deg = LED_ANGLE_MAX_DEG;
    }

    center = (uint32_t)(((angle_deg - LED_ANGLE_MIN_DEG) /
                         (LED_ANGLE_MAX_DEG - LED_ANGLE_MIN_DEG)) *
                        (float)(LED_COUNT - 1U) + 0.5f);

    if (center < (LED_ACTIVE_COUNT / 2U)) {
        first = 0U;
    } else if (center > ((LED_COUNT - 1U) - (LED_ACTIVE_COUNT / 2U))) {
        first = LED_COUNT - LED_ACTIVE_COUNT;
    } else {
        first = center - (LED_ACTIVE_COUNT / 2U);
    }

    for (i = 0U; i < LED_COUNT; i++) {
        GPIO_PinState state = ((i >= first) &&
                               (i < (first + LED_ACTIVE_COUNT)))
                              ? LED_ON_STATE : LED_OFF_STATE;

        HAL_GPIO_WritePin(leds[i].port, leds[i].pin, state);
    }
}
