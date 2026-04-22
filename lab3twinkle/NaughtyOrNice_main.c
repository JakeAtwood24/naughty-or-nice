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

// Andy's Modification
// GPIO Port E (External LEDs on PE0, PE1, PE2, PE3)
#define GPIO_PORTE_DATA_R   (*((volatile uint32_t *)0x400243FC))
#define GPIO_PORTE_DIR_R    (*((volatile uint32_t *)0x40024400))
#define GPIO_PORTE_DEN_R    (*((volatile uint32_t *)0x4002451C))
// Needed these extra defines to make everything work.
#define GPIO_PORTE_AFSEL_R (*((volatile uint32_t *)0x40024420)) // Alt function
#define GPIO_PORTE_AMSEL_R (*((volatile uint32_t *)0x40024528)) // Analog mode
#define GPIO_PORTE_PCTL_R  (*((volatile uint32_t *)0x4002452C)) // Port control

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

// Andy's Modification
// LED bitmask patterns
#define LED1 0x01 // PE0
#define LED2 0x02 // PE1
#define LED3 0x04 // PE2
#define LED4 0x08 // PE3
#define ALL_OFF 0x00

// ---------------------------------------------------------------------------
// FSM state definitions
// "Twinkle twinkle little star how I wonder what you are"
// ---------------------------------------------------------------------------
typedef enum {
    S_TWINKLE1 = 0,  // "Twinkle"  — C C
    S_TWINKLE2,      // "twinkle"  — G G
    S_LITTLE,        // "little"   — A A
    S_STAR,          // "star"     — G (half)
    S_HOW_I,         // "how I"    — F F
    S_WONDER,        // "wonder"   — E E
    S_WHAT_YOU,      // "what you" — D D
    S_ARE            // "are"      — C (half)
} SongState;

// ---------------------------------------------------------------------------
// Hardware init
// ---------------------------------------------------------------------------
// Andy Modified
// Modified PWM_Init to take LEDs ports
void Init_All(void) {
    // Enable Clock for Port B (bit 1) and Port E (bit 4)
    SYSCTL_RCGCGPIO_R |= 0x12; 
    while ((SYSCTL_RCGCGPIO_R & 0x12) == 0) {} // Wait for clock to stabilize

    // Configure Port E for LEDs (PE0-PE3)
    GPIO_PORTE_AMSEL_R &= ~0x0F;      // Disable analog 
    GPIO_PORTE_AFSEL_R &= ~0x0F;      // Regular GPIO mode
    GPIO_PORTE_PCTL_R  &= ~0x0000FFFF; // Clear PCTL for PE0-PE3
    GPIO_PORTE_DIR_R   |= 0x0F;       // Set as output
    GPIO_PORTE_DEN_R   |= 0x0F;       // Enable digital
    
    // Configure Port B for PWM (PB6)
    SYSCTL_RCGCPWM_R |= 0x01;
    GPIO_PORTB_AFSEL_R |= 0x40;
    GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R & 0xF0FFFFFF) | 0x04000000;
    GPIO_PORTB_DEN_R |= 0x40;

    // PWM Setup
    PWM0_0_CTL_R = 0;
    PWM0_0_GENA_R = 0x8C;
    PWM0_0_CTL_R = 1;
}

// ---------------------------------------------------------------------------
// SysTick busy-wait
// ---------------------------------------------------------------------------
// SysTick delay — waits for exactly 'ticks' clock cycles
void SysTick_Wait(uint32_t reload) {
  NVIC_ST_CTRL_R = 0; // Disable SysTick during setup
  NVIC_ST_RELOAD_R = reload - 1; // Set reload value
  NVIC_ST_CURRENT_R = 0; // Clear current value and COUNTFLAG
  NVIC_ST_CTRL_R = 0x05; // Enable + core clock, no interrupt
  while ((NVIC_ST_CTRL_R & COUNTFLAG) == 0) {} // Wait until count reaches 0
}

// Andy's Modified
// keynum is piano key number, dur is duration in seconds
void note(int keynum, float dur, uint32_t led_mask) {
    uint32_t freq  = (uint32_t)(440.0 * pow(2.0, (keynum - 49) / 12.0));
    uint32_t ticks = (uint32_t)(dur * SYSCLK_HZ);

    // LEDs and Frequency
    GPIO_PORTE_DATA_R = led_mask; 
    PWM0_0_LOAD_R = (SYSCLK_HZ / freq) - 1;
    PWM0_0_CMPA_R = PWM0_0_LOAD_R / 2;
    PWM0_ENABLE_R |= 0x01; // Ensure sound is on

    SysTick_Wait(ticks);

    // Eliminate the high pitch in between each note
    GPIO_PORTE_DATA_R = 0;
    PWM0_ENABLE_R &= ~0x01; // Turn off sound bit
    SysTick_Wait((uint32_t)(0.05f * SYSCLK_HZ));
}

// ---------------------------------------------------------------------------
// FSM tick — executes one state, returns the next state
// This was originally just the Twinkle() function
// ---------------------------------------------------------------------------
SongState FSM_Tick(SongState current) {
    switch (current) {
        case S_TWINKLE1:   // "Twinkle" — C C
            note(note_C, QUARTER, LED1);
            note(note_C, QUARTER, LED1);
            return S_TWINKLE2;

        case S_TWINKLE2:   // "twinkle" — G G
            note(note_G, QUARTER, LED2);
            note(note_G, QUARTER, LED2);
            return S_LITTLE;

        case S_LITTLE:     // "little" — A A
            note(note_A, QUARTER, LED3);
            note(note_A, QUARTER, LED3);
            return S_STAR;

        case S_STAR:       // "star" — G (half note), all LEDs flash
            note(note_G, HALF, LED1|LED2|LED3|LED4);
            return S_HOW_I;

        case S_HOW_I:      // "how I" — F F
            note(note_F, QUARTER, LED4);
            note(note_F, QUARTER, LED4);
            return S_WONDER;

        case S_WONDER:     // "wonder" — E E
            note(note_E, QUARTER, LED3);
            note(note_E, QUARTER, LED3);
            return S_WHAT_YOU;

        case S_WHAT_YOU:   // "what you" — D D
            note(note_D, QUARTER, LED2);
            note(note_D, QUARTER, LED2);
            return S_ARE;

        case S_ARE:        // "are" — C (half note)
            note(note_C, HALF, LED1);
            return S_TWINKLE1;  // Loop back to the beginning

        default:
            return S_TWINKLE1;  // Safety fallback
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(void) {
    Init_All();

    SongState state = S_TWINKLE1;  // Initial state

    while (1) {
        state = FSM_Tick(state);   // Execute current state, advance to next
    }
}
