#ifndef LED_H
#define LED_H

/* angle_deg is mapped from -20 to +20 degrees; four LEDs are lit. */
void led_show_angle(float angle_deg);
void led_clear(void);

/*
void led_test_init(void);
void led_test_loop(void);
*/

#endif /* LED_H */
