#ifndef LED_H
#define LED_H

#ifdef __cplusplus
extern "C" {
#endif

/* angle_deg is mapped from -20 to +20 degrees; four LEDs are lit. */
void led_show_angle(float angle_deg);
void led_clear(void);

/*
void led_test_init(void);
void led_test_loop(void);
*/

#ifdef __cplusplus
}
#endif

#endif /* LED_H */
