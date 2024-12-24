#ifndef CPUFREQ_H
#define	CPUFREQ_H

#define F_CPU 20000000
#define SAMPLES_PER_BIT 16
#define USART_BAUD_VALUE(BAUD_RATE) (uint16_t) ((F_CPU << 6) / (((float) SAMPLES_PER_BIT) * (BAUD_RATE)) + 0.5)

// Set up CPU frequency to the correct value
void cpufreq_init(void);

#endif

