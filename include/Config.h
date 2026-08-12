#ifndef CONFIG_H
#define CONFIG_H

#define FIR_TAPS   31
#define FIR_CHUNK  512
#define NUM_SAMPLES  22528   


#define COEFF_BASE   0x0000              
#define AUDIO_BASE   (COEFF_BASE + FIR_TAPS)  

#endif
