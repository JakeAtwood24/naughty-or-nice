#include <stdint.h>
#include <math.h>

// System control registers
#define SYSCTL_RCGCGPIO_R (*((volatile uint32_t *)0x400FE608)) // GPIO clock
#define SYSCTL_RCGCPWM_R (*((volatile uint32_t *)0x400FE640)) // PWM clock

// GPIO Port B registers (base: 0x40005000)
#define GPIO_PORTB_AFSEL_R (*((volatile uint32_t *)0x40005420)) // Alt function
#define GPIO_PORTB_DEN_R (*((volatile uint32_t *)0x4000551C)) // Digital enable
#define GPIO_PORTB_AMSEL_R (*((volatile uint32_t *)0x40005528)) // Analog mode
#define GPIO_PORTB_PCTL_R (*((volatile uint32_t *)0x4000552C)) // Port control

// PWM Module 0, Generator 0 registers (base: 0x40028000)
#define PWM0_ENABLE_R (*((volatile uint32_t *)0x40028008)) // PWM output enable
#define PWM0_0_CTL_R (*((volatile uint32_t *)0x40028040)) // Generator control
#define PWM0_0_LOAD_R (*((volatile uint32_t *)0x40028050)) // Load (period)
#define PWM0_0_CMPA_R (*((volatile uint32_t *)0x40028058)) // Compare A (duty)
#define PWM0_0_GENA_R (*((volatile uint32_t *)0x40028060)) // Generator A action

// SysTick registers (Cortex-M core)
#define NVIC_ST_CTRL_R (*((volatile uint32_t *)0xE000E010)) // Control/Status
#define NVIC_ST_RELOAD_R (*((volatile uint32_t *)0xE000E014)) // Reload value
#define NVIC_ST_CURRENT_R (*((volatile uint32_t *)0xE000E018)) // Current value
#define COUNTFLAG (1U << 16) // Bit 16 of NVIC_ST_CTRL_R

#define SYSCLK_HZ 16000000 // 16 MHz default clock (no PLL)

// Note key numbers on an 88-key piano
#define note_C  40  // Middle C
#define note_D  42
#define note_E  44
#define note_F  45
#define note_G  47
#define note_A  49  // A4 = 440Hz
#define note_B  51
#define note_HC 52  // High C

// Quarter/half note durations in seconds, the "f" makes it a float
#define QUARTER 0.5 // This ends up being .5 seconds, which is a quarter note at 120 bpm
#define HALF    1.0 // This ends up being 1 second, which is just a half note at 120 bpm

void PWM_Init(void) {
  SYSCTL_RCGCGPIO_R |= 0x02; // Enable clock for Port B
  SYSCTL_RCGCPWM_R |= 0x01; // Enable clock for PWM Module 0
  while ((SYSCTL_RCGCGPIO_R & 0x02) == 0) {} // Wait for clock
  while ((SYSCTL_RCGCPWM_R & 0x01) == 0) {} // Wait for clock

  // Configure PB6 as PWM output (alternate function 4 = M0PWM0)
  GPIO_PORTB_AFSEL_R |= 0x40; // Enable alt function on PB6
  GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R & 0xF0FFFFFF) | 0x04000000; // AF4
  GPIO_PORTB_DEN_R |= 0x40; // Enable digital I/O on PB6
  GPIO_PORTB_AMSEL_R &= ~0x40; // Disable analog on PB6

  // PWM generator 0: count down, 440 Hz, 50% duty
  PWM0_0_CTL_R = 0; // Disable during setup
  PWM0_0_GENA_R = 0x8C; // High at LOAD, low at CMPA
  PWM0_0_LOAD_R = (SYSCLK_HZ / 440) - 1; // Replaced with 440 (same freq)
  PWM0_0_CMPA_R = PWM0_0_LOAD_R / 2; // 50% duty cycle
  PWM0_0_CTL_R = 1; // Enable generator
  PWM0_ENABLE_R |= 0x01; // Enable PWM output on PB6
}

// SysTick delay — waits for exactly 'ticks' clock cycles
void SysTick_Wait(uint32_t reload) {
  NVIC_ST_CTRL_R = 0; // Disable SysTick during setup
  NVIC_ST_RELOAD_R = reload - 1; // Set reload value
  NVIC_ST_CURRENT_R = 0; // Clear current value and COUNTFLAG
  NVIC_ST_CTRL_R = 0x05; // Enable + core clock, no interrupt
  while ((NVIC_ST_CTRL_R & COUNTFLAG) == 0) {} // Wait until count reaches 0
}

// keynum is piano key number (1-88), dur is duration in seconds
void note(int keynum, float dur) {
  uint32_t freq  = (uint32_t)(440.0 * pow(2.0, (keynum - 49) / 12.0)); // Frequency from key number
  uint32_t ticks = (uint32_t)(dur * SYSCLK_HZ); // Convert seconds to clock ticks

  PWM0_0_CTL_R  = 0;
  PWM0_0_LOAD_R = (SYSCLK_HZ / freq) - 1; // Sets the load to the correct frequency for the note
  PWM0_0_CMPA_R = PWM0_0_LOAD_R / 2;  // 50% duty
  PWM0_0_CTL_R  = 1;
  SysTick_Wait(ticks); // Holds the note for the desired length

  // Brief silence between notes so they sound distinct (Everything will sound like a half note if this is not done)
  PWM0_0_CMPA_R = PWM0_0_LOAD_R; // 0% duty = silence
  SysTick_Wait((uint32_t)(0.05f * SYSCLK_HZ)); // Just wanted a brief silence to differentiate between the notes
}

// I split the song into two segments, since it is a mirrored song structure (Explained further further down)
void Play_Chorus(void) {
    note(note_C, QUARTER); note(note_C, QUARTER);
    note(note_G, QUARTER); note(note_G, QUARTER);
    note(note_A, QUARTER); note(note_A, QUARTER);
    note(note_G, HALF);
    note(note_F, QUARTER); note(note_F, QUARTER);
    note(note_E, QUARTER); note(note_E, QUARTER);
    note(note_D, QUARTER); note(note_D, QUARTER);
    note(note_C, HALF);
}
void Play_Bridge(void) {
    note(note_G, QUARTER); note(note_G, QUARTER);
    note(note_F, QUARTER); note(note_F, QUARTER);
    note(note_E, QUARTER); note(note_E, QUARTER);
    note(note_D, HALF);
}

// The song has a mirrored structure, so the chorus plays at the beggining and end, and the bridge twice inbetween.
void twinkle(void) {
    Play_Chorus();
    Play_Bridge();
    Play_Bridge();
    Play_Chorus();
}

int main(void) {
    PWM_Init();
    while (1) {
        twinkle(); // Play Twinkle Twinkle Little Star in full
        SysTick_Wait((2 * SYSCLK_HZ));  // Pause before repeating
    }
}