#define F_CPU 16000000
#define ARRAY_LEN 6
#define STR_LEN 16
#define IND_CONST 1010647491 // Approximate result of 4 pi^2 16MHz^2 C / 1'000
#define DELTA_MAX 10

#define CAP_OFFSET 8

#define CAP_CONST_PRE_1_RES_220	    2600000// * power = 3
#define CAP_CONST_PRE_1_RES_10K    55000000// * power = 3
#define CAP_CONST_PRE_64_RES_220   44000000// * power = 0
#define CAP_CONST_PRE_1024_RES_220  2400000// * power = 0

#define CAP_TASK 0
#define IND_TASK 1

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include "lcd.h"

uint8_t task = CAP_TASK;

uint8_t i, flag, power, divisor, debug, button_flag;

uint16_t val[ARRAY_LEN];
uint16_t dmin, dmax;

uint16_t cap_delay, cap_delay_debug;

uint32_t davg, davgsquared, L, Ldecimal, denominator;
uint32_t capacitance;

char string[STR_LEN];

void cap_task();
void ind_task();


/* Interrupt service routine called by TC1 for every input capture signal.
 * For Inductor:
 *    Resets counter on first time being called.
 *    Fills 'val' array with every subsequent call with absoulute time.
 *    Updates flag every call to know how many times it has been called.
 * For Capacitor:
 *    Puts the calue of the Input Capture Register of TC1 (timer value
 *    at the moment of IRQ) in the 'cap_delay' vaiable. Then increments 'flag'.
 */
ISR(TIMER1_CAPT_vect)
{
	if(task == CAP_TASK)
		cap_delay = ICR1;

	else{
		if(flag == 0)
			TCNT1 = 0;

		else if(flag <= ARRAY_LEN)
			val[flag - 1] = ICR1;
	}

	flag++;
}

/* ISR called by External Pin Interrupt request.
 * Sets the variable 'button_flag'.
 */
ISR(INT0_vect)
{
	button_flag = 1;
}


/* Function for making delays based on '_delay_ms()'. It prints on screen
 * the string "Measuring..." while checking every 200ms if the External
 * Interrupt Pin has been triggered (and the ISR set 'button_flag'.
 * If so, returns, it's the calling function's job to check 'button_flag'.
 *
 * Arg: milli_seconds: the number of milliseconds to delay.
 *    Should be a multiple of 200.
 */
void measuring_delay_ms(uint16_t milli_seconds)
{
	Lcd4_Set_Cursor(0, 2);
	Lcd4_Write_String("Measuring");
	for(i = 1; i <= milli_seconds / 200; i++)
	{
		if(button_flag) return;

		_delay_ms(200);

		if(i % 4)
			Lcd4_Write_Char('.');
		else{
			Lcd4_Set_Cursor(0, 11);
			Lcd4_Write_String("   ");
			Lcd4_Set_Cursor(0, 11);
		}
	}

}


/* Main function: Sets common registers for all measurement types and then
 * stays in infinite loop, calling the measuring function according to
 * variable 'task' which stores the type of measurement currently acrive.
 */
void main()
{
	// Pins A0 - A5 outputs
	DDRC = 0b00111111;
	Lcd4_Init();
	Lcd4_Clear();
	Lcd4_Set_Cursor(0, 4);
	Lcd4_Write_String("LC Meter");

	// Enable global interrupt
	SREG |= (1 << 7);

	// Setup of AC
	ACSR &= ~(1 << ACD);// Enable AC
	ACSR &= ~(1 << ACBG);// Disable bandgap reference
	ADCSRB |= (1 << ACME);// Enable Mux
	ADCSRA &= ~(1 << ADEN);// Disable ADC

	// Setup of time TC1
	ACSR |= (1 << ACIC);// Select AC as TC1 input capture trigger
	PRR &= ~(1 << PRTIM1);// Disable Power reduction to TC1
	TIMSK1 |= (1 << ICIE1);// Input capture interrupt enable for TC1

	// External interrupt setup
	EICRA |= (1 << ISC01);// Interrupt 0 request on falling edge
	EIMSK |= (1 << INT0);// Enable INT0


	while(1){
		
		switch(task){

			case CAP_TASK:
				cap_task();
				break;

			case IND_TASK:
				ind_task();
				break;
		}
		if(button_flag){
			task = !task;
			button_flag = 0;
		}
	}
}

/* Function that meaures capacitance and displays value.
 * Sets specific registers for the measurement then performs measurements.
 * and corrects measuring parameters based on the result (cap too big or small)
 * Return after displaying the measurement.
 */
void cap_task()
{

	// Select ADC6 pin as inverting input of AC
	ADMUX |= 0b00000110;
	ADMUX &= 0b11111110;

	// Use prescaler = 1
	TCCR1B |= 0b00000001;
	TCCR1B &= 0b11111001;

	denominator = CAP_CONST_PRE_1_RES_220;
	power = 3;

	// Pin D8 HIGH
	PORTB &= ~(1 << PB0);

	// Pin D8 Output (res = 220)
	DDRB |= (1 << PB0);

	_delay_ms(1000);

	flag = 0;

	TCNT1 = 0;

	PORTB |= (1 << PB0);

	_delay_ms(4);

	// Pin D8 Input
	DDRB &= ~(1 << PB0);

	
	if(flag != 0 && cap_delay < 1200){
	
		// Prescaler still = 1.

		denominator = CAP_CONST_PRE_1_RES_10K;
		power = 3;

		// Pin D9 HIGH
		PORTB &= ~(1 << PB1);

		// Pin D9 Output (res = 10k)
		DDRB |=  (1 << PB1);

		_delay_ms(100);

		flag = 0;

		TCNT1 = 0;

		PORTB |= (1 << PB1);

		_delay_ms(4);
		
		// Pin D9 Inputs
		DDRB &= ~(1 << PB1);

	}

	cap_delay -= CAP_OFFSET;

	if(flag == 0){

		// Pin D8 (res = 220) still output

		TCCR1B |= 0b00000011;// Use prescaler = 64
		TCCR1B &= 0b00000011;

		denominator = CAP_CONST_PRE_64_RES_220;
		power = 0;

		// Pin D8 LOW
		PORTB &= ~(1 << PB0);
		// Pin D8 Output
		DDRB  |= (1 << PB0);

		// Wait 1 second
		// If in the meantime button is pressed,
		// return within 100ms to change task.
		measuring_delay_ms(1000);
		if(button_flag) return;

		flag = 0;

		TCNT1 = 0;

		// Pin D8 HIGH
		PORTB |= (1 << PB0);

		_delay_ms(250);

		// PIn D8 Input
		DDRB  &= ~(1 << PB0);


		if(flag == 0){

			TCCR1B |= 0b00000101;// Use prescaler = 1024
			TCCR1B &= 0b00000101;

			denominator = CAP_CONST_PRE_1024_RES_220;
			power = 0;

			// Pin D8 LOW
			PORTB &= ~(1 << PB0);
			// Pin D8 Output
			DDRB  |= (1 << PB0);

			// Wait 8 seconds
			// If in the meantime button is pressed,
			// return within 100ms to change task.
			measuring_delay_ms(8000);
			if(button_flag) return;
	
			flag = 0;
	
			TCNT1 = 0;
	
			PORTB |= (1 << PB0);
	
			// Wait 4 seconds
			// If in the meantime button is pressed,
			// return within 100ms to change task.
			measuring_delay_ms(4000);
			if(button_flag) return;
		
			// PIn D8 Input
			DDRB  &= ~(1 << PB0);
		}
	}

	Lcd4_Clear();
	Lcd4_Set_Cursor(0, 2);
	Lcd4_Write_String("Capacitance:");

	if(cap_delay == 0)
	{
		Lcd4_Set_Cursor(1, 6);
		Lcd4_Write_String("Open");
		return;
	}
	if(flag == 0)
	{
		Lcd4_Set_Cursor(1, 2);
		Lcd4_Write_String("Too big");
		return;
	}

		
	// To make 'capacitance' >= 100 always possible
	if(cap_delay != 0)
	while(cap_delay < 1000){
		cap_delay *= 10;
		power++;
	}

	capacitance = ((uint32_t)cap_delay) / denominator;
	for(; capacitance < 100; power++)
	{
		denominator /= 10;
		capacitance = ((uint32_t)cap_delay) / denominator;
	}
	if(capacitance >= 1000){
		capacitance /= 10;
		power--;
	}

	sprintf(string, "%d", (uint16_t)capacitance);

	// Insert '.' at correct position inside the string representing the decimal
	if(power % 3)
	{
		for(i = STR_LEN - 1; i >= 4 - (power % 3); i--)
			string[i] = string[i - 1];

		string[i] = '.';
	}

	Lcd4_Set_Cursor(1, 5);
	Lcd4_Write_String(string);

	if(power >= 12)
		Lcd4_Write_String("pF");
	else if(power >= 9)
		Lcd4_Write_String("nF");
	else if(power >= 6)
		Lcd4_Write_String("uF");
	else if(power >= 3)
		Lcd4_Write_String("mF");

}



/* Function that meaures inductance and displays value.
 * Sets specific registers for the measurement and performs measurements.
 * Return after displaying the measurement.
 */
void ind_task()
{

	// Pin D1 as output (transistor base)
	DDRD |= (1 << PD1);

	// Setup of AC
	ADMUX |= 0b00000111;// Select ADC7 pin as inverting input of AC

	// Setup of time TC1
	TCCR1B |= 0b00000001;// Use prescaler = 1
	TCCR1B &= 0b11111001;

	_delay_ms(500);

	flag = 0;

	// Set Pin D1 to HIGH
	PORTD |= (1 << PD1);

	// Delay for approx 250 ns
	asm("nop");
	asm("nop");
	asm("nop");


	TCNT1 = 0;
	// Pin D1 to LOW
	PORTD &= ~(1 << PD1);

	_delay_ms(100);

	// 'Divisor' is the number of valid periods measured
	divisor = ARRAY_LEN - 1;

	dmax = 0;
	dmin = 0xffff;

	// Loop through all values (periods)
	for(i = ARRAY_LEN - 1; i > 0; i--)
	{
		// if a value is null, kepp track with "divisor" and skip loop
		if(val[i] == 0){
			divisor = i - 1;
			continue;
		}
		// Find maximum period
		if(val[i] - val[i - 1] > dmax)
			dmax = val[i] - val[i - 1];

		// Find minumum period
		if(val[i] - val[i -	1] < dmin)
			dmin = val[i] - val[i - 1];
	}

	Lcd4_Clear();
	Lcd4_Set_Cursor(0, 2);
	Lcd4_Write_String("Inductance:");

	// Calculate average period of oscillation
	davg = (uint32_t)(val[divisor] - val[0]) / ((uint32_t)divisor);

	// Set variables for calculating inductance
	davgsquared = davg * davg;
	denominator = IND_CONST;

	// Find power of denominator such that L is n*100 e-m for decimal formatting
	L = davgsquared / denominator;
	for(power = 0; L < 100; power++)
	{
		denominator /= 10;
		L = davgsquared / denominator;
	}
	// Because of rounding error, L can go from 99 -> 1000, must catch!
	if(L >= 1000){
		L /=10;
		power--;
	}

	sprintf(string, "%d", (uint16_t)L);

	// Insert '.' at correct position inside the string representing the decimal
	if(power % 3)
	{
		for(i = STR_LEN - 1; i >= 4 - (power % 3); i--)
			string[i] = string[i - 1];

		string[i] = '.';
	}

	Lcd4_Set_Cursor(1, 5);

	// Check validity of the measurements
	if(dmin <= davg &&\
		davg <= dmax &&\
		dmin > 0 &&\
		((uint16_t)davg) / (dmax - dmin) >= DELTA_MAX)
	{
		Lcd4_Write_String(string);

		if(power >= 9)
			Lcd4_Write_String("nH");
		else if(power >= 6)
			Lcd4_Write_String("uH");
		else if(power >= 3)
			Lcd4_Write_String("mH");
	}

	else if(dmin == 0xffff)
		Lcd4_Write_String("Open");

	else
		Lcd4_Write_String("Error");

	for(i = 0; i < ARRAY_LEN; i++)
		val[i] = 0;

}
