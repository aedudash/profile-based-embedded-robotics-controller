// Author : Aidan Dudash
// Profile Secure Robotics Controller

// ========== Definitions ==========

// Clock and Headers
#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>

// UART info
#define BAUD 9600
#define UBRR_VAL ((F_CPU / (16UL * BAUD)) - 1)

// Servo info
#define SERVO_MIN 1000 // 0.5 ms
#define SERVO_MAX 5000 // 2.5 ms

// EEPROM profile address info
#define MAX_PW_LEN  16
#define ADMIN_ADDR  0x0000
#define USER1_ADDR  0x0013
#define USER2_ADDR  0x0026

// Pin configurations
#define BUTTON_DDR  DDRB
#define BUTTON_PIN  PINB
#define BUTTON_PORT PORTB

#define BUTTON_ADMIN PB2
#define BUTTON_USER1 PB3
#define BUTTON_USER2 PB4

#define BUZZER_DDR  DDRD
#define BUZZER_PORT PORTD
#define BUZZER_PIN  PD3

#define RESET_DDR  DDRD
#define RESET_PIN  PIND
#define RESET_PORT PORTD
#define RESET_BTN  PD6

// ========== Function prototypes ==========

extern void myDelay_ms(short int delay_time);
void uart_send_string(const char* str);
void set_leds(uint8_t green_on);
void buzz(uint16_t duration_ms);

// ========== Global Variables/Arrays ==========

volatile uint16_t adc_val = 0;         // Stores latest ADC reading
volatile uint8_t emergency_flag = 0;   // Emergency stop flag
volatile uint8_t access_granted = 0;   // Access flag
volatile uint8_t selected_profile = 0; // User selected profile
volatile uint16_t profile_addr = 0;	   // EE profile location

// Admin = 180*, User1 ~ 90*, User2 ~ 45*
const uint16_t servo_min_pw[3] = {1000, 2000, 2500}; // Admin, User1, User2
const uint16_t servo_max_pw[3] = {5000, 4000, 3500}; // Admin, User1, User2

// ========== Initilization Functions ===========

void light_init(void) {
	// Configure PD4 and PD5 as outputs for Red and Green LEDs
	DDRD |= (1 << PD4) | (1 << PD5);	 // Set PD4 and PD5 as outputs
	PORTD &= ~((1 << PD4) | (1 << PD5)); // Ensure both are initially off
}

void button_init(void) {
	BUTTON_DDR &= ~((1 << BUTTON_ADMIN) | (1 << BUTTON_USER1) | (1 << BUTTON_USER2)); // Inputs
	BUTTON_PORT |= (1 << BUTTON_ADMIN) | (1 << BUTTON_USER1) | (1 << BUTTON_USER2);   // Internal pull-ups
	RESET_DDR &= ~(1 << RESET_BTN);  // Set as input
	RESET_PORT |= (1 << RESET_BTN);  // Enable internal pull-up
}

void buzzer_init(void) {
	BUZZER_DDR |= (1 << BUZZER_PIN); // Set buzzer pin as output
}

// Initialize UART for 9600 baud
void uart_init(void) {
	UBRR0H = (unsigned char)(UBRR_VAL >> 8);
	UBRR0L = (unsigned char)(UBRR_VAL);
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);   // Enable receiver and transmitter
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8-bit data, 1 stop bit, no parity
}

// Configure ADC for PC0 (ADC0), interrupt enabled
// Prescaler = 8 to yield 50 Hz PWM frequency for servo control
void adc_init() {
	ADMUX = (1 << REFS0);  // AVcc reference, PC0
	ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADPS2) | (1 << ADPS1);
	ADCSRA |= (1 << ADSC); // Start first conversion
}

// Set PB1 (OC1A) as PWM output for servo
void timer1_init() {
	DDRB |= (1 << PB1);
	// Configure Timer1 for 50 Hz Fast PWM mode
	TCCR1A = (1 << COM1A1) | (1 << WGM11);
	TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11); // Prescaler = 8
	ICR1 = 39999;      // TOP for 20ms period (50Hz)
	OCR1A = 3000;      // ~1.5ms starting pulse (neutral position)
}

// Initialize INT0 on PD2 for emergency input
void int0_init(void) {
	DDRD &= ~(1 << PD2);     // Set PD2 as input
	PORTD |= (1 << PD2);     // Enable internal pull-up
	myDelay_ms(50);			 // Stabilizing delay
	EICRA |= (1 << ISC01);   // Trigger on falling edge
	EICRA &= ~(1 << ISC00);  // Ensure only ISC01 is set (falling edge)
	EIFR |= (1 << INTF0);    // Clear any pending INT0 flag
	EIMSK |= (1 << INT0);    // Enable INT0 interrupt
}

void setup_init(void) {
	uart_init();      // Initialize UART
	light_init();     // Initialize access lights
	button_init();    // Initialize buttons
	buzzer_init();    // Initialize buzzer
	set_leds(0);      // Red light ON
}

// ========== MY Functions ==========

void reset_all_passwords(void) {
	uart_send_string("!! RESETTING ALL PASSWORDS !!\r\n");

	for (uint16_t i = 0; i < MAX_PW_LEN; i++) {
		// Admin
		while (EECR & (1 << EEPE));
		EEAR = ADMIN_ADDR + i;
		EEDR = 0xFF;
		EECR |= (1 << EEMPE);
		EECR |= (1 << EEPE);

		// User1
		while (EECR & (1 << EEPE));
		EEAR = USER1_ADDR + i;
		EEDR = 0xFF;
		EECR |= (1 << EEMPE);
		EECR |= (1 << EEPE);

		// User2
		while (EECR & (1 << EEPE));
		EEAR = USER2_ADDR + i;
		EEDR = 0xFF;
		EECR |= (1 << EEMPE);
		EECR |= (1 << EEPE);
	}
	
	uart_send_string("EEPROM passwords cleared.\r\n");
	buzz(500); // Long buzz to indicate reset complete
}

uint8_t select_profile(void) {
	uart_send_string("Select Profile:\r\n");
	uart_send_string("   Press [Admin] Button\r\n");
	uart_send_string("   Press [User1] Button\r\n");
	uart_send_string("   Press [User2] Button\r\n");

	while (1) {
		if (!(BUTTON_PIN & (1 << BUTTON_ADMIN))) {
			uart_send_string("Admin Profile Selected\r\n");
			return 0;
		}
		if (!(BUTTON_PIN & (1 << BUTTON_USER1))) {
			uart_send_string("User1 Profile Selected\r\n");
			return 1;
		}
		if (!(BUTTON_PIN & (1 << BUTTON_USER2))) {
			uart_send_string("User2 Profile Selected\r\n");
			return 2;
		}
	}
}

// Map a selected profile to its EEPROM address
uint16_t get_profile_eeprom_addr(uint8_t profile) {
	switch (profile) {
		case 0: return ADMIN_ADDR;
		case 1: return USER1_ADDR;
		case 2: return USER2_ADDR;
		default: return ADMIN_ADDR; // Fallback
	}
}

// Buzz the passive buzzer for a given duration in milliseconds
void buzz(uint16_t duration_ms) {
	BUZZER_PORT |= (1 << BUZZER_PIN);  // Turn buzzer ON
	myDelay_ms(duration_ms);           // Wait
	BUZZER_PORT &= ~(1 << BUZZER_PIN); // Turn buzzer OFF
}

// Wait for transmit buffer to be empty, then send a byte
void uart_transmit(char data) {
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = data;
}

// Wait for and receive a byte from UART
char uart_receive(void) {
	while (!(UCSR0A & (1 << RXC0)));
	return UDR0;
}

// Send a null-terminated string over UART
void uart_send_string(const char* str) {
	while (*str) {
		uart_transmit(*str++);
	}
	while (!(UCSR0A & (1 << TXC0))); // Wait for last byte to finish transmission
	UCSR0A |= (1 << TXC0);           // Clear TX Complete flag
}

// Compare two strings; return 0 if equal
int my_strcmp(const char* s1, const char* s2) {
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}
	return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// Get a string input from UART with echo masking as '*'
void uart_get_string(char* buffer, uint8_t max_len) {
	uint8_t i = 0;
	while (i < (max_len - 1)) {
		char c = uart_receive();
		if (c == '\r' || c == '\n') break; // Stop on Enter
		buffer[i++] = c;
		uart_transmit('*');				   // Echo as '*'
	}
	buffer[i] = '\0';					   // Null-terminate
	uart_send_string("\r\n");
}

// Read a block of bytes from EEPROM
void my_eeprom_read_block(void* dest, uint16_t addr, uint8_t len) {
	uint8_t* d = (uint8_t*)dest;
	for (uint8_t i = 0; i < len; i++) {
		while (EECR & (1 << EEPE));        // Wait for any ongoing EEPROM write
		EEAR = addr + i;                   // Set EEPROM address
		EECR |= (1 << EERE);               // Trigger EEPROM read
		d[i] = EEDR;                       // Store result in buffer
	}
}

// Safely write a block of bytes to EEPROM with verification
uint8_t my_eeprom_update_block(const void* src, uint16_t addr, uint8_t len) {
	const uint8_t* s = (const uint8_t*)src;

	for (uint8_t i = 0; i < len; i++) {
		while (EECR & (1 << EEPE));  // Wait for any ongoing EEPROM write to finish

		// Read current EEPROM value at this address
		EEAR = addr + i;
		EECR |= (1 << EERE);
		
		// Only write if the data is different (preserves EEPROM lifespan)
		if (EEDR != s[i]) {
			EEAR = addr + i;         // Set EEPROM address again (required before write)
			EEDR = s[i];             // Load new data into data register
			EECR |= (1 << EEMPE);    // Enable master write
			EECR |= (1 << EEPE);     // Start EEPROM write

			while (EECR & (1 << EEPE));  // Wait for write to complete

			// Re-read and verify the write was successful
			EEAR = addr + i;
			EECR |= (1 << EERE);
			if (EEDR != s[i]) {
				return 0; // Verification failed — abort the write
			}
		}
	}

	return 1; // Success: all bytes written and verified
}

// Check if a password has been stored in EEPROM
uint8_t password_exists(uint16_t eeprom_addr) {
	char c;
	my_eeprom_read_block(&c, eeprom_addr, 1);
	return (c != 0xFF && c != '\0');

}

void set_leds(uint8_t green_on) {
	if (green_on) {
		PORTD |= (1 << PD5);
		PORTD &= ~(1 << PD4);
		} else {
		PORTD |= (1 << PD4);
		PORTD &= ~(1 << PD5);
	}
}

// Set a new password and store it in EEPROM
void set_new_password(uint16_t eeprom_addr) {
	char new_pass[MAX_PW_LEN];
	uint8_t index = 0;
	uint8_t enter_pressed = 0;
	
	// Flush any leftovers in RX buffer
	while (UCSR0A & (1 << RXC0)) (void)UDR0;
	uart_send_string("Create new password:\r\n");

	// Collect user input character-by-character
	while (!enter_pressed && index < (MAX_PW_LEN - 1)) {
		char c = uart_receive();

		// Enter key pressed — finalize input
		if (c == '\r' || c == '\n') {
			enter_pressed = 1;
			new_pass[index] = '\0'; // Null-terminate input string
			} else {
			new_pass[index++] = c;
			uart_transmit('*');     // Echo masked input
		}
	}
	uart_send_string("\r\n");

	// Reject empty passwords
	if (index == 0) {
		uart_send_string("Password too short. Try again.\r\n");
		return;
	}
	// Attempt to store password in EEPROM
	if (!my_eeprom_update_block((const void*)new_pass, eeprom_addr, MAX_PW_LEN)) {
		uart_send_string("EEPROM write error. Password not saved.\r\n");
		return;
	}
	uart_send_string("Password saved.\r\n");
}

// Run login attempt with comparison to stored EEPROM password
uint8_t security_protocol(uint16_t eeprom_addr) {
	char input[MAX_PW_LEN];
	char stored[MAX_PW_LEN];
	uint8_t index = 0, failed_attempts = 0;

	my_eeprom_read_block((void*)stored, eeprom_addr, MAX_PW_LEN);
	uart_send_string("Enter password:\r\n");

	while (1) {
		char c = uart_receive();

		// Ignore CR or LF unless we're ending the line
		if (c == '\r' || c == '\n') {
			if (index == 0) continue;  // prevent processing empty lines
			input[index] = '\0';

			if (my_strcmp(input, stored) == 0) {
				uart_send_string("\r\nAccess Granted\r\n");
				return 1;
				} else {
				failed_attempts++;
				uart_send_string("\r\nAccess Denied\r\n");
				buzz(100); 

				if (failed_attempts >= 3) {
					uart_send_string("Too many attempts. Locked out.\r\n");
					buzz(100);
					return 0;
				}
				index = 0;
				uart_send_string("Enter password:\r\n");
			}
			} else if (index < MAX_PW_LEN - 1) {
			input[index++] = c;
			uart_transmit('*');
		}
	}
}

// Function for emergency/lockout protocol
void emergency_lockout_protocol(void) {
	cli();				  // Shutdown "robot"
	access_granted = 0;	  // Revoke access
	emergency_flag = 0;   // Prevent re-triggering
	set_leds(0);		  // Red light ON
	uart_send_string("!!! EMERGENCY TRIGGERED !!!\r\n");
	buzz(100);			  // Audible alert
}

// ========== ISRs ==========

// INT0 interrupt – emergency trigger
ISR(INT0_vect) {
		emergency_flag = 1;
}

// ADC interrupt - joystick control
ISR(ADC_vect) {
	adc_val = ADC;

	// Map ADC (0-1023) to pulse width (1000–5000 us)
	uint16_t raw_pw = 1000 + ((uint32_t)adc_val * 4000) / 1023;

	// Clamp based on selected profile's range
	uint16_t min_pw = servo_min_pw[selected_profile];
	uint16_t max_pw = servo_max_pw[selected_profile];

	if (raw_pw < min_pw) raw_pw = min_pw;
	if (raw_pw > max_pw) raw_pw = max_pw;

	OCR1A = raw_pw;
	ADCSRA |= (1 << ADSC); // Start next ADC conversion
}

// ========== MAIN ===========

int main(void) {
	while (1) {
		cli();		  // Disable global interrupts during setup
		setup_init(); // Setup system

		// === Profile Selection and Authentication ===
		selected_profile = select_profile();
		profile_addr = get_profile_eeprom_addr(selected_profile);

		// Prompt for password setup if EEPROM is blank
		if (!password_exists(profile_addr)) {
			set_new_password(profile_addr); 
		}

		// Perform security login
		access_granted = security_protocol(profile_addr);
		
		// === Optional: Allow Admin to reset all passwords ===
		if (selected_profile == 0 && access_granted) {
			uart_send_string("Press 'R' to reset all passwords or any other key to skip:\r\n");

			myDelay_ms(500); // Give terminal time to catch up
			
			// Flush any pre-existing characters in the RX buffer
			while (UCSR0A & (1 << RXC0)) (void)UDR0;

			char choice = uart_receive();

			if (choice == 'R' || choice == 'r') {
				uart_send_string("Are you sure? Press 'Y' to confirm reset:\r\n");
				myDelay_ms(500); // Give time for user input

				// Flush RX buffer again
				while (UCSR0A & (1 << RXC0)) (void)UDR0;

				char confirm = uart_receive();

				if (confirm == 'Y' || confirm == 'y') {
					reset_all_passwords();
					uart_send_string("Restarting system...\r\n");
					myDelay_ms(100);
					continue; // Restart main loop after reset
					} else {
					uart_send_string("Reset cancelled.\r\n");
				}
			}
		}
		
		// === If access is granted, enable robot functions ===
		if (access_granted) {
			set_leds(1);	// Green light ON
			adc_init();		// ADC for joystick input
			timer1_init();	// Timer1 PWM for servo
			int0_init();	// INT0 (PD2) emergency trigger
			sei();			// Global interrupt enable
		}

		// === Main Loop with Emergency Check ===
		while (1) {
			if (emergency_flag) {
				emergency_lockout_protocol();

				// Wait for reset button (PD6) press to restart system
				while (1) {
					if (!(RESET_PIN & (1 << RESET_BTN))) {   // Active low
						myDelay_ms(50);                      // Simple debounce
						if (!(RESET_PIN & (1 << RESET_BTN))) {
							emergency_flag = 0;
							uart_send_string("System logout. Restarting...\r\n");
							myDelay_ms(50); // Let TX complete before restarting
							break;			// Exit emergency wait
						}
					}
				}
				break; // Restart from top of main
			}
		}
	}
}





/*
===================ADC========================
// ADC interrupt – updates PWM based on joystick input
ISR(ADC_vect) {
	adc_val = ADC;
	// Map 0-1023 ADC value to 1k-5k pulse width (0.5ms-2.5ms)
	OCR1A = 1000 + ((uint32_t)adc_val * 4000) / 1023;
	ADCSRA |= (1 << ADSC); // Start next conversion
}

ISR(ADC_vect) {
	adc_val = ADC;

	// Map ADC (0-1023) to pulse width (1000–5000 us)
	uint16_t raw_pw = 1000 + ((uint32_t)adc_val * 4000) / 1023;

	// Clamp based on selected profile's range
	uint16_t min_pw = servo_min_pw[selected_profile];
	uint16_t max_pw = servo_max_pw[selected_profile];

	if (raw_pw < min_pw) raw_pw = min_pw;
	if (raw_pw > max_pw) raw_pw = max_pw;

	OCR1A = raw_pw;
	ADCSRA |= (1 << ADSC); // Start next ADC conversion
}

====================================

// Send a null-terminated string over UART
void uart_send_string(const char* str) {
	while (*str) {
		uart_transmit(*str++);
	}
	while (!(UCSR0A & (1 << TXC0))); // Wait for last byte to finish transmission
	UCSR0A |= (1 << TXC0);           // Clear TX Complete flag
}


void int0_init(void) {
	DDRD &= ~(1 << PD2);     // Set PD2 as input
	PORTD |= (1 << PD2);     // Enable internal pull-up

	// Small delay to let input stabilize before enabling interrupt
	myDelay_ms(50);          

	EICRA |= (1 << ISC01);   // Trigger on falling edge
	EICRA &= ~(1 << ISC00);  // Ensure only ISC01 is set (falling edge)

	EIFR |= (1 << INTF0);    // Clear any pending INT0 flag
	EIMSK |= (1 << INT0);    // Enable INT0 interrupt
}


int main(void) {
	cli();		  // Disable global interrupts during setup
	setup_init(); // Setup system
	
	// === Profile Selection and Authentication ===
	selected_profile = select_profile();
	profile_addr = get_profile_eeprom_addr(selected_profile);

	// Prompt for password setup if EEPROM is blank
	if (!password_exists(profile_addr)) {
		set_new_password(profile_addr); // Prompt and save password
	}

	// Perform security login
	access_granted = security_protocol(profile_addr);

	// === If granted, enable robot functions ===
	if (access_granted) {
		set_leds(1);		  // Green light ON
		adc_init();			  // ADC for joystick input
		timer1_init();		  // Timer1 PWM for servo
		int0_init();		  // INT0 (PD2) emergency trigger
		sei();				  // Global interrupt enable
	}

	// === Main Loop with Emergency Check ===
	while (1) {
		if (emergency_flag) {
			emergency_lockout_protocol();
		}
	}
}


*/