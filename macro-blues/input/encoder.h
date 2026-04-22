#ifndef ENCODER_H
#define ENCODER_H

void encoder_init(void);

/* Called from GPIOTE ISR on encoder pin edges */
void encoder_isr_update(void);

/* Called from GPIOTE ISR on button press (HiToLo edge) */
void encoder_btn_isr_update(void);

/* Call once per main-loop iteration.
 * Returns accumulated steps since last call: +N = CW, -N = CCW, 0 = none */
int encoder_poll(void);

/* Returns 1 (and clears flag) if button was pressed since last call */
int encoder_btn_fell(void);

#endif /* ENCODER_H */
