#ifndef FRAMETIMER_H
#define	FRAMETIMER_H

// Initialize frame timer
void frametimer_init(void);

// Start the frame timer
void frametimer_start(void);

// Use polling to wait until the timer next fires
void frametimer_wait(void);

#endif

