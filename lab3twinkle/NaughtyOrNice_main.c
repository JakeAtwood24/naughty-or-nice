#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

//// ===============================================
//// IF YOU ARE RUNNING THIS PROGRAM MAKE SURE TO CHANGE PORTF IN startup_ccs.c TO GPIOPortF_Handler and have extern void GPIOPortF_Handler(void);
//// IN THE EXTERNAL DECLARATION AT THE TOP OF PROGRAM
//// YOU HAVE BEEN WARNED.
//// Andy W.
//// ===============================================
//// Hi, Andy W again, Update on May 4th 2026
//// You will need to switch two things in startup.ccs
//// 1st. Declare this in external declarations: extern void UART0_Handler(void);
//// 2nd. Find the UART0 in the NVIC and replace it with: UART0_Handler
//// ===============================================

//// ===============================================
//// Registers, Set-Up, Bitmasks, and misc Constants
//// ===============================================
// System control registers
#define SYSCTL_RCGCGPIO_R (*((volatile uint32_t *)0x400FE608))  // GPIO clock
#define SYSCTL_RCGCPWM_R (*((volatile uint32_t *)0x400FE640))   // PWM clock

// GPIO Port B registers (base: 0x40005000)
// Jake added bottom 2 for more lights
#define GPIO_PORTB_AFSEL_R (*((volatile uint32_t *)0x40005420)) // Alt function
#define GPIO_PORTB_DEN_R (*((volatile uint32_t *)0x4000551C))   // Digital enable
#define GPIO_PORTB_AMSEL_R (*((volatile uint32_t *)0x40005528)) // Analog mode
#define GPIO_PORTB_PCTL_R (*((volatile uint32_t *)0x4000552C))  // Port control
#define GPIO_PORTB_DATA_R (*((volatile uint32_t *)0x400053FC))
#define GPIO_PORTB_DIR_R  (*((volatile uint32_t *)0x40005400))  // Direction register

// Andy's Modification
// GPIO Port E (External LEDs on PE0, PE1, PE2, PE3)
#define GPIO_PORTE_DATA_R   (*((volatile uint32_t *)0x400243FC))
#define GPIO_PORTE_DIR_R    (*((volatile uint32_t *)0x40024400))
#define GPIO_PORTE_DEN_R    (*((volatile uint32_t *)0x4002451C))
// Needed these extra defines to make everything work.
#define GPIO_PORTE_AFSEL_R (*((volatile uint32_t *)0x40024420)) // Alt function
#define GPIO_PORTE_AMSEL_R (*((volatile uint32_t *)0x40024528)) // Analog mode
#define GPIO_PORTE_PCTL_R  (*((volatile uint32_t *)0x4002452C)) // Port control

// Sara's Modification
// GPIO Port F (Button SW1)
#define GPIO_PORTF_DATA_R (*((volatile uint32_t *)0x400253FC))  //SW1
#define GPIO_PORTF_DIR_R (*((volatile uint32_t *)0x40025400))   // Direction register
#define GPIO_PORTF_AFSEL_R (*((volatile uint32_t *)0x40025420)) // Alternate function
#define GPIO_PORTF_DEN_R (*((volatile uint32_t *)0x4002551C))   // Digital enable
#define GPIO_PORTF_PUR_R (*((volatile uint32_t *)0x40025510))   // Pull-up resistor
#define GPIO_PORTF_LOCK_R (*((volatile uint32_t *)0x40025520))  // Lock register
#define GPIO_PORTF_CR_R (*((volatile uint32_t *)0x40025524))    // Commit register

// Andy's Modifications
// UART0 Register Defines
#define SYSCTL_RCGCUART_R (*((volatile uint32_t *)0x400FE618))
#define UART0_DR_R        (*((volatile uint32_t *)0x4000C000))
#define UART0_FR_R        (*((volatile uint32_t *)0x4000C018))
#define UART0_IBRD_R      (*((volatile uint32_t *)0x4000C024))
#define UART0_FBRD_R      (*((volatile uint32_t *)0x4000C028))
#define UART0_LCRH_R      (*((volatile uint32_t *)0x4000C02C))
#define UART0_CTL_R       (*((volatile uint32_t *)0x4000C030))
#define GPIO_PORTA_AFSEL_R (*((volatile uint32_t *)0x40004420))
#define GPIO_PORTA_PCTL_R  (*((volatile uint32_t *)0x4000452C))
#define GPIO_PORTA_DEN_R   (*((volatile uint32_t *)0x4000451C))
// New UART Registers
#define UART0_IM_R        (*((volatile uint32_t *)0x4000C038)) // Interrupt Mask
#define UART0_ICR_R       (*((volatile uint32_t *)0x4000C044)) // Interrupt Clear
#define UART0_MIS_R       (*((volatile uint32_t *)0x4000C040)) // Masked Interrupt Status 

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
#define PWM0_0_CTL_R (*((volatile uint32_t *)0x40028040))  // Generator control
#define PWM0_0_LOAD_R (*((volatile uint32_t *)0x40028050)) // Load (period)
#define PWM0_0_CMPA_R (*((volatile uint32_t *)0x40028058)) // Compare A (duty)
#define PWM0_0_GENA_R (*((volatile uint32_t *)0x40028060)) // Generator A action

// SysTick registers 
#define NVIC_ST_CTRL_R (*((volatile uint32_t *)0xE000E010))     // Control/Status
#define NVIC_ST_RELOAD_R (*((volatile uint32_t *)0xE000E014))   // Reload value
#define NVIC_ST_CURRENT_R (*((volatile uint32_t *)0xE000E018))  // Current value
#define COUNTFLAG (1U << 16) // Bit 16 of NVIC_ST_CTRL_R

// Switching the dynamic memory allocation to a fixed memory allocation for efficiency and dedicated memory - Andy W.
#define BUFFER_MAX 10
// Define the system clock (no PLL)
#define SYSCLK_HZ 16000000 // 16 MHz default clock (no PLL)

// Andy's Modification
// Jake Update for 10 lights
// LED bitmask patterns
#define LED1  0x001 // PE0
#define LED2  0x002 // PE1
#define LED3  0x004 // PE2
#define LED4  0x008 // PE3
#define LED5  0x010 // PE4
#define LED6  0x020 // PE5
#define LED7  0x040 // PB0
#define LED8  0x080 // PB1
#define LED9  0x100 // PB2
#define LED10 0x200 // PB3
#define ALL_OFF 0x00
//Sara's Modfifications
#define SW1 0x10

//// ===============================================
//// Music Constants
//// ===============================================
// Jake Note Updates
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
#define note_HCS 53 // High C sharp

// Jake Rhythm Updates
// Quarter/half note durations in seconds, the "f" makes it a float
#define EIGHTH 0.25
#define QUARTER 0.5 // This ends up being .5 seconds, which is a quarter note at 120 bpm
#define DOT_Q 0.75
#define HALF 1.0    // This ends up being 1 second, which is just a half note at 120 bpm
#define DOT_H 1.5   // Dotted half
#define WHOLE 2.0   // 2.0 * HALF
// Adjusted BPM for the Grinch
#define GRINCH_TEMPO 1.4f

//// ===============================================
//// FSM Setup
//// ===============================================
// Moved this function to the top because the order of operation matter.
typedef enum {
   // Idle Song (Deck the Halls)
    S_DECK_HALLS = 0, S_FA_LA_1, S_TIS_SEASON, S_FA_LA_2, 
    S_DON_WE_NOW, S_FA_LA_3, S_TROLL_YULE, S_FA_LA_4,

    // Naughty Song (The Grinch)
    S_GRINCH_1 = 20, // Starting at 20 just to keep them visually separate
    S_GRINCH_2, S_GRINCH_3, S_GRINCH_4,

    // Nice Song (Jingle Bells)
    S_JINGLE_1 = 30, // Offset to keep it organized
    S_JINGLE_2, S_JINGLE_3, S_JINGLE_4,S_JINGLE_FINISH
} SongState;

// Andy's Modfications
// State Variables and volatile functions
volatile uint8_t is_playing = 0; // 0 = Idle, 1 = Song is active
volatile uint8_t is_paused = 0;  // 0 = Running, 1 = Frozen
volatile SongState current_state = S_DECK_HALLS; // Turn this into a global because the program needs to be able to switch on the fly.

//// ===============================================
//// Fixed-sized Queue-based Buffer
//// ===============================================
// Jordan's Implemenation 
// Andy's Modfication
typedef struct {
    size_t MAX_SIZE;
    volatile int arr[BUFFER_MAX]; // Changed from pointer to fixed array
    size_t size; 
} buffer;

void init_buffer(volatile buffer* buf) {
    buf->MAX_SIZE = BUFFER_MAX; 
    buf->size = 0;
    // No malloc needed with fixed memory
}

// push_buffer and pop_buffer stay almost identical, 
// but they are now safer because the memory is locked in.
void push_buffer(volatile buffer* buf, int value) {
    if (buf->size < buf->MAX_SIZE) {
        buf->arr[buf->size] = value; 
        buf->size++;
    }
}

uint8_t pop_buffer(volatile buffer* buf) {
    if (buf->size == 0) return 254; // Using a literal instead of INT8_MAX for simplicity
    
    int first = buf->arr[0]; 

    // Shift elements
    size_t i;
    for (i = 0; i < (buf->size - 1); i++) {
        buf->arr[i] = buf->arr[i + 1]; 
    }

    buf->size--;
    return (uint8_t)first;  
}

void clear_buffer ( volatile buffer* buf) {
    buf->size = 0;
}

void cleanup_buffer ( volatile buffer* buf) {
    buf->size = 0;
    buf->MAX_SIZE = 0; 
    free(buf);
}

volatile buffer state_buffer; 

//// ===============================================
//// Init/Handler/Helper Functions
//// ===============================================
// Andy Modified
// Modified PWM_Init to take LEDs ports
void Init_All(void) {
    // Enable Clock for Port B (bit 1) and Port E (bit 4)
    SYSCTL_RCGCGPIO_R |= 0x12; 
    while ((SYSCTL_RCGCGPIO_R & 0x12) == 0) {} // Wait for clock to stabilize

    // Configure Port E for LEDs (PE0-PE5)
    GPIO_PORTE_AMSEL_R &= ~0x3F;        // Disable analog on PE0-PE5
    GPIO_PORTE_AFSEL_R &= ~0x3F;        // Regular GPIO mode
    GPIO_PORTE_PCTL_R  &= ~0x00FFFFFF;  // Clear PCTL for PE0-PE5
    GPIO_PORTE_DIR_R   |= 0x3F;         // Set as output
    GPIO_PORTE_DEN_R   |= 0x3F;         // Enable digital
    
    // Configure Port B: PB0-PB3 as GPIO LEDs, PB6 as PWM
    SYSCTL_RCGCPWM_R |= 0x01;

    GPIO_PORTB_AMSEL_R &= ~0x0F;        // Disable analog on PB0-PB3
    GPIO_PORTB_AFSEL_R &= ~0x0F;        // Regular GPIO for PB0-PB3
    GPIO_PORTB_AFSEL_R |=  0x40;        // Alt function for PB6 (PWM)
    GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R & 0xF0FFFF00) | 0x04000000; // PWM on PB6, clear PB0-PB3
    GPIO_PORTB_DIR_R   |= 0x0F;         // PB0-PB3 output
    GPIO_PORTB_DEN_R   |= 0x4F;         // Digital enable PB0-PB3 and PB6

    // PWM Setup
    PWM0_0_CTL_R = 0;
    PWM0_0_GENA_R = 0x8C;
    PWM0_0_CTL_R = 1;
}

// Jake Modification
// Helper to write a 10-bit led_mask to both ports
static inline void set_leds(uint32_t led_mask) {
    GPIO_PORTE_DATA_R = (led_mask & 0x3F);         // PE5:PE0
    GPIO_PORTB_DATA_R = (led_mask >> 6) & 0x0F;    // PB3:PB0
}

// Andy's Modifications
// UART Helper Functions
// Just the above Init_all, I'm initalizing Port A to be strictly UART.
void UART_Init(void) {
    SYSCTL_RCGCUART_R |= 0x01;  // Enable UART0
    SYSCTL_RCGCGPIO_R |= 0x01;  // Enable Port A
    while((SYSCTL_RCGCGPIO_R & 0x01) == 0);

    UART0_CTL_R &= ~0x01;       // Disable UART
    UART0_IBRD_R = 8;           // 115,200 baud for 16MHz clock
    UART0_FBRD_R = 44;
    UART0_LCRH_R = 0x70;        // 8-bit, FIFO enabled

    // Enable Receive Interrupts - Andy W
    // These lines setup for a bidirectional UART
    UART0_IM_R |= 0x10;         // Arm RXRIS (Receive Interrupt)
    NVIC_EN0_R |= (1 << 5);     // UART0 is Interrupt #5 in NVIC
    
    UART0_CTL_R |= 0x01;        // Enable UART (TX and RX)
    
    GPIO_PORTA_AFSEL_R |= 0x03; // PA0, PA1 alt function
    GPIO_PORTA_DEN_R |= 0x03;
    GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R & 0xFFFFFF00) + 0x00000011;
}

// SysTick delay — waits for exactly 'ticks' clock cycles
void SysTick_Wait(uint32_t reload) {
    NVIC_ST_CTRL_R = 0;                             // Disable SysTick during setup
    NVIC_ST_RELOAD_R = reload - 1;                  // Set reload value
    NVIC_ST_CURRENT_R = 0;                          // Clear current value and COUNTFLAG
    NVIC_ST_CTRL_R = 0x05;                          // Enable + core clock, no interrupt
    while ((NVIC_ST_CTRL_R & COUNTFLAG) == 0) {}    // Wait until count reaches 0
}

// Sara's Modification
// Button input
void PortF_Init_Buttons(void) {
    SYSCTL_RCGCGPIO_R |= 0x20;
    while ((SYSCTL_RCGCGPIO_R & 0x20) == 0) {}
    GPIO_PORTF_LOCK_R = 0x4C4F434B; // Unlock PF4 only
    GPIO_PORTF_CR_R |= 0x10;
    GPIO_PORTF_DIR_R &= ~0x10;      // PF4 input
    GPIO_PORTF_AFSEL_R &= ~0x10;    // PF4 GPIO
    GPIO_PORTF_PUR_R |= 0x10;       // Pull-up on PF4
    GPIO_PORTF_DEN_R |= 0x10;       // Digital enable PF4

    // Andy's Modified
    // Lines added to handle interrupts
    GPIO_PORTF_IS_R  &= ~0x10;  // PF4 is edge-sensitive
    GPIO_PORTF_IBE_R &= ~0x10;  // PF4 is not both edges
    GPIO_PORTF_IEV_R &= ~0x10;  // PF4 falling edge event 
    GPIO_PORTF_ICR_R  = 0x10;   // Clear any prior flag
    GPIO_PORTF_IM_R  |= 0x10;   // Arm interrupt on PF4 

    // Andy Modified
    // Enable Port F interrupt in NVIC
    NVIC_EN0_R |= (1 << 30);
}

//// ===============================================
//// Bidirectional UART Functions
//// ===============================================
// This is the function that allows us to send sentences through the serial console
void UART_OutString(char *pt) {
    while(*pt) {
        while((UART0_FR_R & 0x20) != 0); // Wait if TX FIFO full
        UART0_DR_R = *pt++;
    }
}

// Andy W.
// This function will handle all our remote operation through UART
// Just two inputs for now "g" to set the naughty flag and "j" to set the nice flag
void UART0_Handler(void) {
    UART0_ICR_R = 0x10; // Clear the interrupt flag
    
    char c = (char)(UART0_DR_R & 0xFF); // Read the character
    
    if (c == 'g' || c == 'G') {
        UART_OutString("\r\n[UART] Remote Trigger: Grinch Mode!\r\n");
        push_buffer(&state_buffer, S_GRINCH_1);
    } 
    else if (c == 'j' || c == 'J') {
        UART_OutString("\r\n[UART] Remote Trigger: Jingle Mode!\r\n");
        push_buffer(&state_buffer, S_JINGLE_1);
    }
}

// The Interrupt Service Routine
// Updated, 4/27/2026, Adding in the "randomness" logic here.
// Idea: Since the MC runs so many cycles each second, the human input will act the random input. The chance of a person
// Hitting the same number again is so low that this randomness would be okay I think. - Andy W.
void GPIOPortF_Handler(void) {
    GPIO_PORTF_ICR_R = 0x10;
    
    if (!is_playing) {
       UART_OutString("\r\n[SYSTEM] Button Pressed! Analyzing soul...\r\n");
        
        // Random Logic
        // Grab the lower bit of the SysTick timer for a 50/50 chance
        // If we are currently in the background song (Deck the Halls)
        if ( (current_state >= S_DECK_HALLS && current_state <= S_FA_LA_4) && state_buffer.size == 0) {
            UART_OutString("\r\n[SYSTEM] Analyzing soul...\r\n");
            uint32_t rng_seed = NVIC_ST_CURRENT_R; 
            if (rng_seed % 2 == 0) {
                UART_OutString("[RESULT] Status: NICE. Playing Jingle Bells.\r\n");
                push_buffer(&state_buffer, S_JINGLE_1);

            } 
            else {
                UART_OutString("[RESULT] Status: NAUGHTY! Playing The Grinch.\r\n");
                push_buffer(&state_buffer, S_GRINCH_1);

            }
            is_paused = 0; // Make sure we aren't paused when the new song starts
        } 
        else {
            // If the Grinch or Nice song is already playing, toggle pause
            is_paused = !is_paused;
            if(is_paused){
                UART_OutString("[LOG] Music Paused.\r\n");
            }
            else{
                UART_OutString("[LOG] Music Resumed.\r\n");
            }
        }
    }
}

//// ===============================================
//// Music Functions (Note + Rest)
//// ===============================================
// Andy's Modified
// keynum is piano key number, dur is duration in seconds
// Jake Modified to use new set_leds() helper
void note(int keynum, float dur, uint32_t led_mask) {
    uint32_t freq  = (uint32_t)(440.0 * pow(2.0, (keynum - 49) / 12.0));
    uint32_t total_ticks = (uint32_t)(dur * SYSCLK_HZ);

    // LEDs and Frequency
    set_leds(led_mask);
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
            set_leds(ALL_OFF);
            while (is_paused); 
            
            // Restore hardware state on resume
            PWM0_ENABLE_R |= 0x01;
            set_leds(led_mask);
        }
    }

    // Silence in between notes
    set_leds(ALL_OFF);
    PWM0_ENABLE_R &= ~0x01;
    SysTick_Wait((uint32_t)(0.05f * SYSCLK_HZ));
}

// Jake Modified Rest Function
// Since 0 doesn't work as a "rest" when passed through the note function had to make a rest function.
// Jake Modified to use new set_leds() helper
void rest(float dur) {
    PWM0_ENABLE_R &= ~0x01; // Turn off sound
    set_leds(ALL_OFF);  // LEDs off

    SysTick_Wait((uint32_t)(dur * SYSCLK_HZ));
}

//// ===============================================
//// FSM Details and Transitions
//// ===============================================
SongState FSM_Tick(SongState current) {
    switch (current) {
        case S_DECK_HALLS:
            note(note_G, DOT_Q, LED1|LED6); note(note_F, EIGHTH, LED2|LED7);
            note(note_E, QUARTER, LED3|LED8); note(note_D, QUARTER, LED4|LED9);
            note(note_C, QUARTER, LED1|LED10); note(note_D, QUARTER, LED2|LED6);
            note(note_E, QUARTER, LED3|LED7); note(note_C, QUARTER, LED4|LED8);
            return S_FA_LA_1;

        case S_FA_LA_1:
            note(note_D, EIGHTH, LED1|LED2|LED6|LED7); note(note_E, EIGHTH, LED3|LED4|LED8|LED9);
            note(note_F, EIGHTH, LED1|LED2|LED6|LED7); note(note_D, EIGHTH, LED3|LED4|LED8|LED9);
            note(note_E, DOT_Q, LED1|LED2|LED3|LED4|LED5|LED6|LED7|LED8|LED9|LED10); note(note_D, EIGHTH, LED1|LED2|LED3|LED4|LED5|LED6|LED7|LED8|LED9|LED10);
            note(note_C, QUARTER, LED1|LED5); note(note_LB, QUARTER, LED2|LED6);
            note(note_C, QUARTER, LED1|LED3|LED7|LED9);
            return S_TIS_SEASON;

        case S_TIS_SEASON:
            // Same melody as Deck the Halls
            note(note_G, DOT_Q, LED1|LED6); note(note_F, EIGHTH, LED2|LED7);
            note(note_E, QUARTER, LED3|LED8); note(note_D, QUARTER, LED4|LED9);
            note(note_C, QUARTER, LED1|LED10); note(note_D, QUARTER, LED2|LED6);
            note(note_E, QUARTER, LED3|LED7); note(note_C, QUARTER, LED4|LED8);
            return S_FA_LA_2;

        case S_FA_LA_2:
            // Repeats the Fa-La-La phrase
            note(note_D, EIGHTH, LED1|LED2|LED6|LED7); note(note_E, EIGHTH, LED3|LED4|LED8|LED9);
            note(note_F, EIGHTH, LED1|LED2|LED6|LED7); note(note_D, EIGHTH, LED3|LED4|LED8|LED9);
            note(note_E, DOT_Q, LED1|LED2|LED3|LED4|LED5|LED6|LED7|LED8|LED9|LED10); note(note_D, EIGHTH, LED1|LED2|LED3|LED4|LED5|LED6|LED7|LED8|LED9|LED10);
            note(note_C, QUARTER, LED1|LED5); note(note_LB, QUARTER, LED2|LED6);
            note(note_C, QUARTER, LED1|LED3|LED7|LED9);
            return S_DON_WE_NOW;

        case S_DON_WE_NOW:
            note(note_D, DOT_Q, LED1|LED6); note(note_E, EIGHTH, LED2|LED7);
            note(note_F, QUARTER, LED3|LED8); note(note_D, QUARTER, LED4|LED9);
            note(note_E, DOT_Q, LED1|LED6); note(note_F, EIGHTH, LED2|LED7);
            note(note_G, QUARTER, LED3|LED8); note(note_D, QUARTER, LED4|LED9);
            return S_FA_LA_3;

        case S_FA_LA_3:
            note(note_E, EIGHTH, LED1|LED6); note(note_F, EIGHTH, LED2|LED7);
            note(note_G, QUARTER, LED3|LED8); note(note_A, EIGHTH, LED4|LED9);
            note(note_B, EIGHTH, LED3|LED8); note(note_HC, QUARTER, LED2|LED4|LED7|LED9);
            note(note_B, QUARTER, LED1|LED5); note(note_A, QUARTER, LED2|LED6);
            note(note_G, HALF, LED1|LED2|LED3|LED4|LED5|LED6|LED7|LED8|LED9|LED10);
            return S_TROLL_YULE;

        case S_TROLL_YULE:
            // Final repeat of the main theme
            note(note_G, DOT_Q, LED1|LED6); note(note_F, EIGHTH, LED2|LED7);
            note(note_E, QUARTER, LED3|LED8); note(note_D, QUARTER, LED4|LED9);
            note(note_C, QUARTER, LED1|LED10); note(note_D, QUARTER, LED2|LED6);
            note(note_E, QUARTER, LED3|LED7); note(note_C, QUARTER, LED4|LED8);
            return S_FA_LA_4;

        case S_FA_LA_4:
            // Final Fa-La-La
            note(note_D, EIGHTH, LED1|LED2|LED6|LED7); note(note_E, EIGHTH, LED3|LED4|LED8|LED9);
            note(note_F, EIGHTH, LED1|LED2|LED6|LED7); note(note_D, EIGHTH, LED3|LED4|LED8|LED9);
            note(note_E, DOT_Q, LED1|LED2|LED3|LED4|LED5|LED6|LED7|LED8|LED9|LED10); note(note_D, EIGHTH, LED1|LED2|LED3|LED4|LED5|LED6|LED7|LED8|LED9|LED10);
            note(note_C, QUARTER, LED1|LED5); note(note_LB, QUARTER, LED2|LED6);
            note(note_C, QUARTER, LED1|LED3|LED7|LED9);
            return S_DECK_HALLS; // Loop back to start

        case S_GRINCH_1: // "You're a mean one..."
            note(note_F,  EIGHTH * GRINCH_TEMPO, LED1|LED10); note(note_G,  EIGHTH * GRINCH_TEMPO, LED2|LED9);
            note(note_A, EIGHTH * GRINCH_TEMPO, LED3|LED8); rest(QUARTER);
            note(note_D,  DOT_Q * GRINCH_TEMPO, LED4|LED7);
            return S_GRINCH_2;

        case S_GRINCH_2: // "...Mr. Grinch"
            note(note_F,  EIGHTH * GRINCH_TEMPO, LED2|LED9); note(note_A, EIGHTH * GRINCH_TEMPO, LED3|LED4|LED7|LED8);
            note(note_G,  DOT_Q * GRINCH_TEMPO, LED1|LED5); rest(HALF);
            return S_GRINCH_3;

        case S_GRINCH_3: // "You really are a heel"
            note(note_D,  EIGHTH * GRINCH_TEMPO, LED1|LED10); note(note_A, DOT_Q * GRINCH_TEMPO, LED2|LED9);
            note(note_A,  EIGHTH * GRINCH_TEMPO, LED3|LED8); note(note_B,  DOT_Q * GRINCH_TEMPO, LED4|LED7);
            note(note_B, EIGHTH * GRINCH_TEMPO, LED1|LED2|LED3|LED4|LED5|LED6|LED7|LED8|LED9|LED10); note(note_HCS, HALF * GRINCH_TEMPO, LED1|LED2|LED3|LED4|LED5|LED6|LED7|LED8|LED9|LED10);
            return S_DECK_HALLS;

        case S_JINGLE_1: // "Jingle bells, jingle bells,"
            note(note_E, QUARTER, LED1|LED6); note(note_E, QUARTER, LED2|LED7);
            note(note_E, HALF,    LED3|LED4|LED8|LED9); note(note_E, QUARTER, LED1|LED6);
            note(note_E, QUARTER, LED2|LED7); note(note_E, HALF,    LED3|LED4|LED8|LED9);
            return S_JINGLE_2;

        case S_JINGLE_2: // "Jingle all the way!"
            note(note_E, QUARTER, LED1|LED6); note(note_G, QUARTER, LED2|LED7);
            note(note_C, QUARTER, LED3|LED8); note(note_D, QUARTER, LED4|LED9);
            note(note_E, WHOLE, LED1|LED2|LED3|LED4|LED5|LED6|LED7|LED8|LED9|LED10);
            return S_JINGLE_3;

        case S_JINGLE_3: // "Oh what fun it is to ride"
            note(note_F, QUARTER, LED1|LED2|LED6|LED7); note(note_F, QUARTER, LED3|LED4|LED8|LED9);
            note(note_F, DOT_Q,   LED1|LED2|LED6|LED7); note(note_F, EIGHTH,  LED3|LED4|LED8|LED9);
            note(note_F, QUARTER, LED1|LED2|LED6|LED7); note(note_E, QUARTER, LED3|LED4|LED8|LED9);
            note(note_E, QUARTER, LED1|LED2|LED6|LED7); note(note_E, EIGHTH,  LED3|LED4|LED8|LED9);
            note(note_E, EIGHTH,  LED1|LED2|LED6|LED7);
            return S_JINGLE_4;

        case S_JINGLE_4: // "In a one-horse open sleigh!"
            note(note_G, QUARTER, LED1|LED6); note(note_G, QUARTER, LED2|LED7);
            note(note_F, QUARTER, LED3|LED8); note(note_D, QUARTER, LED4|LED9);
            note(note_C, DOT_H, LED1|LED2|LED3|LED4|LED5|LED6|LED7|LED8|LED9|LED10);
            return S_DECK_HALLS;

        default:
            return S_DECK_HALLS;
    }
}

//// ===============================================
//// Main
//// ===============================================
int main(void) {
    __asm("    CPSIE  I");  // Enable Interrupts
    Init_All();             // PWM/LEDs
    UART_Init();            // UART Initialization
    PortF_Init_Buttons();   // Button + Interrupt Setup
    // Old: init_buffer(&state_buffer, 10);
    init_buffer(&state_buffer);
    // Start SysTick now so it's "randomizing" in the background
    NVIC_ST_RELOAD_R = 0x00FFFFFF;  // Max 24-bit value
    NVIC_ST_CURRENT_R = 0;          // Clear current to start count
    NVIC_ST_CTRL_R = 0x05;          // Enable timer, core clock

    while (1) {
        // If we aren't paused, keep the music moving
        if (!is_paused) { 
            current_state = FSM_Tick(current_state);
        
            // check the input buffer for new states and overwrite if neccesary
            if (state_buffer.size > 0) {
                current_state = pop_buffer(&state_buffer);
            } 
        }
    }
}
