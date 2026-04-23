#include <stdint.h>
#include <math.h>
////
//// ____________________________________
//// IF YOU ARE RUNNING THIS PROGRAM MAKE SURE TO CHANGE PORTF IN startup_ccs.c TO GPIOPortF_Handler and have extern void GPIOPortF_Handler(void);
//// IN THE EXTERNAL DECLARATION AT THE TOP OF PROGRAM
//// YOU HAVE BEEN WARNED.
//// Andy W.
//// ___________________________________

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

//Sara's Modification
//GPIO Port F (Button SW1)
#define GPIO_PORTF_DATA_R (*((volatile uint32_t *)0x400253FC)) //SW1
#define GPIO_PORTF_DIR_R (*((volatile uint32_t *)0x40025400)) // Direction register
#define GPIO_PORTF_AFSEL_R (*((volatile uint32_t *)0x40025420)) // Alternate function
#define GPIO_PORTF_DEN_R (*((volatile uint32_t *)0x4002551C)) // Digital enable
#define GPIO_PORTF_PUR_R (*((volatile uint32_t *)0x40025510)) // Pull-up resistor
#define GPIO_PORTF_LOCK_R (*((volatile uint32_t *)0x40025520)) // Lock register
#define GPIO_PORTF_CR_R (*((volatile uint32_t *)0x40025524)) // Commit register

// Andy's Modified
// Port F to handle interrupt
#define GPIO_PORTF_IS_R     (*((volatile uint32_t *)0x40025404))
#define GPIO_PORTF_IBE_R    (*((volatile uint32_t *)0x40025408))
#define GPIO_PORTF_IEV_R    (*((volatile uint32_t *)0x4002540C))
#define GPIO_PORTF_IM_R     (*((volatile uint32_t *)0x40025410))
#define GPIO_PORTF_ICR_R    (*((volatile uint32_t *)0x4002541C))
#define NVIC_EN0_R          (*((volatile uint32_t *)0xE000E100))

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
#define note_LB 39
#define note_C  40  // Middle C
#define note_D  42
#define note_E  44
#define note_F  45
#define note_G  47
#define note_A  49  // A4 = 440Hz
#define note_B  51
#define note_HC 52  // High C

// Andy's Modified
// Quarter/half note durations in seconds, the "f" makes it a float
#define EIGHTH 0.25
#define QUARTER 0.5 // This ends up being .5 seconds, which is a quarter note at 120 bpm
#define DOT_Q 0.75
#define HALF    1.0 // This ends up being 1 second, which is just a half note at 120 bpm

// Andy's Modification
// LED bitmask patterns
#define LED1 0x01 // PE0
#define LED2 0x02 // PE1
#define LED3 0x04 // PE2
#define LED4 0x08 // PE3
#define ALL_OFF 0x00
//Sara's Modfifications
#define SW1 0x10

// Andy's Modfications
// State Variables 
volatile uint8_t is_playing = 0; // 0 = Idle, 1 = Song is active
volatile uint8_t is_paused = 0;  // 0 = Running, 1 = Frozen


// Andy Modified
//Modified PWM_Init to take LEDs ports
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

// SysTick delay — waits for exactly 'ticks' clock cycles
void SysTick_Wait(uint32_t reload) {
  NVIC_ST_CTRL_R = 0; // Disable SysTick during setup
  NVIC_ST_RELOAD_R = reload - 1; // Set reload value
  NVIC_ST_CURRENT_R = 0; // Clear current value and COUNTFLAG
  NVIC_ST_CTRL_R = 0x05; // Enable + core clock, no interrupt
  while ((NVIC_ST_CTRL_R & COUNTFLAG) == 0) {} // Wait until count reaches 0
}

// Sara's Modification
// Button input
void PortF_Init_Buttons(void) {
    SYSCTL_RCGCGPIO_R |= 0x20;
    while ((SYSCTL_RCGCGPIO_R & 0x20) == 0) {}
    GPIO_PORTF_LOCK_R = 0x4C4F434B;   // Unlock PF4 only
    GPIO_PORTF_CR_R |= 0x10;
    GPIO_PORTF_DIR_R &= ~0x10;        // PF4 input
    GPIO_PORTF_AFSEL_R &= ~0x10;      // PF4 GPIO
    GPIO_PORTF_PUR_R |= 0x10;         // Pull-up on PF4
    GPIO_PORTF_DEN_R |= 0x10;         // Digital enable PF4

    // Andy's Modified
    // Lines added to handle interrupts
    GPIO_PORTF_IS_R  &= ~0x10;    // PF4 is edge-sensitive
    GPIO_PORTF_IBE_R &= ~0x10;    // PF4 is not both edges
    GPIO_PORTF_IEV_R &= ~0x10;    // PF4 falling edge event 
    GPIO_PORTF_ICR_R  = 0x10;     // Clear any prior flag
    GPIO_PORTF_IM_R  |= 0x10;     // Arm interrupt on PF4 

    // Andy Modified
    // Enable Port F interrupt in NVIC
    NVIC_EN0_R |= (1 << 30);
}

// The Interrupt Service Routine
void GPIOPortF_Handler(void) {
    GPIO_PORTF_ICR_R = 0x10; 
    
    if (!is_playing) {
        is_playing = 1;      // Start the song if it's not running
    } else {
        is_paused = !is_paused; 
        
        // Hardware response if we just paused
        if (is_paused) {
            PWM0_ENABLE_R &= ~0x01; 
            GPIO_PORTE_DATA_R = 0;
        }
    }
}



// Andy's Modified
// keynum is piano key number, dur is duration in seconds
void note(int keynum, float dur, uint32_t led_mask) {
    uint32_t freq  = (uint32_t)(440.0 * pow(2.0, (keynum - 49) / 12.0));
    uint32_t total_ticks = (uint32_t)(dur * SYSCLK_HZ);

    // LEDs and Frequency
    GPIO_PORTE_DATA_R = led_mask; 
    PWM0_0_LOAD_R = (SYSCLK_HZ / freq) - 1;
    PWM0_0_CMPA_R = PWM0_0_LOAD_R / 2;
    PWM0_ENABLE_R |= 0x01; // Ensure sound is on

    // Andy's Modified
    uint32_t elapsed = 0;
    // Check state every 10ms 
    uint32_t chunk = 160000; 

    while (elapsed < total_ticks) {
        if (!is_paused) {
            SysTick_Wait(chunk);
            elapsed += chunk;
        } else {
            // Stay in this loop until is_paused is toggled by the ISR
            PWM0_ENABLE_R &= ~0x01;
            GPIO_PORTE_DATA_R = 0;
            while (is_paused); 
            
            // Restore hardware state on resume
            PWM0_ENABLE_R |= 0x01;
            GPIO_PORTE_DATA_R = led_mask;
        }
    }

    // Silence in between notes
    GPIO_PORTE_DATA_R = 0;
    PWM0_ENABLE_R &= ~0x01;
    SysTick_Wait((uint32_t)(0.05f * SYSCLK_HZ));
}

typedef enum {
    S_DECK_HALLS = 0,   // "Deck the halls with boughs of holly"
    S_FA_LA_1,          // "Fa la la la la, la la la la"
    S_TIS_SEASON,       // "Tis the season to be jolly"
    S_FA_LA_2,          // "Fa la la la la, la la la la"
    S_DON_WE_NOW,       // "Don we now our gay apparel"
    S_FA_LA_3,          // "Fa la la, fa la la, la la la"
    S_TROLL_YULE,       // "Troll the ancient Yule-tide carol"
    S_FA_LA_4           // "Fa la la la la, la la la la"
} SongState;

// Andy's Modified
SongState FSM_Tick(SongState current) {
    switch (current) {
        case S_DECK_HALLS:
            note(note_G, DOT_Q, LED1); note(note_F, EIGHTH, LED2);
            note(note_E, QUARTER, LED3); note(note_D, QUARTER, LED4);
            note(note_C, QUARTER, LED1); note(note_D, QUARTER, LED2);
            note(note_E, QUARTER, LED3); note(note_C, QUARTER, LED4);
            return S_FA_LA_1;

        case S_FA_LA_1:
            note(note_D, EIGHTH, LED1|LED2); note(note_E, EIGHTH, LED3|LED4);
            note(note_F, EIGHTH, LED1|LED2); note(note_D, EIGHTH, LED3|LED4);
            note(note_E, DOT_Q, LED1|LED2|LED3|LED4); note(note_D, EIGHTH, LED1|LED2|LED3|LED4);
            note(note_C, QUARTER, LED1); note(note_LB, QUARTER, LED2);
            note(note_C, QUARTER, LED1|LED3);
            return S_TIS_SEASON;

        case S_TIS_SEASON:
            // Same melody as Deck the Halls
            note(note_G, DOT_Q, LED1); note(note_F, EIGHTH, LED2);
            note(note_E, QUARTER, LED3); note(note_D, QUARTER, LED4);
            note(note_C, QUARTER, LED1); note(note_D, QUARTER, LED2);
            note(note_E, QUARTER, LED3); note(note_C, QUARTER, LED4);
            return S_FA_LA_2;

        case S_FA_LA_2:
            // Repeats the Fa-La-La phrase
            note(note_D, EIGHTH, LED1|LED2); note(note_E, EIGHTH, LED3|LED4);
            note(note_F, EIGHTH, LED1|LED2); note(note_D, EIGHTH, LED3|LED4);
            note(note_E, DOT_Q, LED1|LED2|LED3|LED4); note(note_D, EIGHTH, LED1|LED2|LED3|LED4);
            note(note_C, QUARTER, LED1); note(note_LB, QUARTER, LED2);
            note(note_C, QUARTER, LED1|LED3);
            return S_DON_WE_NOW;

        case S_DON_WE_NOW:
            note(note_D, DOT_Q, LED1); note(note_E, EIGHTH, LED2);
            note(note_F, QUARTER, LED3); note(note_D, QUARTER, LED4);
            note(note_E, DOT_Q, LED1); note(note_F, EIGHTH, LED2);
            note(note_G, QUARTER, LED3); note(note_D, QUARTER, LED4);
            return S_FA_LA_3;

        case S_FA_LA_3:
            note(note_E, EIGHTH, LED1); note(note_F, EIGHTH, LED2);
            note(note_G, QUARTER, LED3); note(note_A, EIGHTH, LED4);
            note(note_B, EIGHTH, LED3); note(note_HC, QUARTER, LED2|LED4);
            note(note_B, QUARTER, LED1); note(note_A, QUARTER, LED2);
            note(note_G, HALF, LED1|LED2|LED3|LED4);
            return S_TROLL_YULE;

        case S_TROLL_YULE:
            // Final repeat of the main theme
            note(note_G, DOT_Q, LED1); note(note_F, EIGHTH, LED2);
            note(note_E, QUARTER, LED3); note(note_D, QUARTER, LED4);
            note(note_C, QUARTER, LED1); note(note_D, QUARTER, LED2);
            note(note_E, QUARTER, LED3); note(note_C, QUARTER, LED4);
            return S_FA_LA_4;

        case S_FA_LA_4:
            // Final Fa-La-La
            note(note_D, EIGHTH, LED1|LED2); note(note_E, EIGHTH, LED3|LED4);
            note(note_F, EIGHTH, LED1|LED2); note(note_D, EIGHTH, LED3|LED4);
            note(note_E, DOT_Q, LED1|LED2|LED3|LED4); note(note_D, EIGHTH, LED1|LED2|LED3|LED4);
            note(note_C, QUARTER, LED1); note(note_LB, QUARTER, LED2);
            note(note_C, QUARTER, LED1|LED3);
            return S_DECK_HALLS; // Loop back to start

        default:
            return S_DECK_HALLS;
    }
}

int main(void) {
    __asm("    CPSIE  I");      // Enable Interrupts
    Init_All();                 // PWM/LEDs
    PortF_Init_Buttons();       // Button + Interrupt Setup
    
    SongState state = S_DECK_HALLS;

    while (1) {
        // If the button interrupt has set is_playing to 1
        if (is_playing) {
            state = FSM_Tick(state);
            // Check if song finished, if so looped back to start
            if (state == S_DECK_HALLS) {
                is_playing = 0; // Stop and wait for button press to restart
            }
        }
    }
}