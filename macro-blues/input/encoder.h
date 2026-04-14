#ifndef ENCODER_H
#define ENCODER_H

void encoder_init(void);

/* Called from GPIOTE ISR on encoder pin edges */
void encoder_isr_update(void);

/* Call once per main-loop iteration.
 * Returns accumulated steps since last call: +N = CW, -N = CCW, 0 = none */
int encoder_poll(void);

#endif /* ENCODER_H */
