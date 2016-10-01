/*
 * -------------------------------------------
 *    MSP432 DriverLib - v3_21_00_05 
 * -------------------------------------------
 *
 * --COPYRIGHT--,BSD,BSD
 * Copyright (c) 2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
/*******************************************************************************
 * MSP432 Port Mapper - Remapping Timer_A CCR
 *
 * Description: This program generates a PWM output on P2.4
 *  (need to finish description)
 *
 * Authors: Sam Litchfield and Aneesa Sonawalla
 * Code sourced from CCS examples including: pmap_timera_redirection,
*******************************************************************************/

/* DriverLib Includes */
#include "driverlib.h"

/* Standard Includes */
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>


/* Port mapper configuration register */
const uint8_t port2_mapping[] =
{
        //Port P2:
        PM_NONE, PM_NONE, PM_NONE, PM_NONE, PM_TA1CCR1A, PM_NONE, PM_NONE,
        PM_NONE
};

/* Timer_A UpDown Mode Configuration Parameter */
const Timer_A_UpDownModeConfig upDownConfig =
{
        TIMER_A_CLOCKSOURCE_SMCLK,              // SMCLK Clock SOurce
        TIMER_A_CLOCKSOURCE_DIVIDER_8,          // SMCLK/1 = 3MHz
        127,                                    // 127 tick period
        TIMER_A_TAIE_INTERRUPT_DISABLE,         // Disable Timer interrupt
        TIMER_A_CCIE_CCR0_INTERRUPT_DISABLE,    // Disable CCR0 interrupt
        TIMER_A_DO_CLEAR                        // Clear value
};

/* Timer_A Continuous Mode Configuration Parameter */
// This isn't used
const Timer_A_ContinuousModeConfig continuousModeConfig =
{
        TIMER_A_CLOCKSOURCE_ACLK,            // ACLK Clock Source
        TIMER_A_CLOCKSOURCE_DIVIDER_3,       // ACLK/1 = 32.768KHz
        TIMER_A_TAIE_INTERRUPT_ENABLE,       // Enable Timer ISR
        TIMER_A_SKIP_CLEAR                   // Skip Clear Counter
};

/* Timer_A Up Mode Configuration Parameter (10 ms interrupts) */
const Timer_A_UpModeConfig upModeConfig =
{
        TIMER_A_CLOCKSOURCE_ACLK,            // ACLK is Clock Source
        TIMER_A_CLOCKSOURCE_DIVIDER_1,       // ACLK/1 = 32.768KHz
		327,								    // Timer Period -> Step freq down to 100hz
        TIMER_A_TAIE_INTERRUPT_ENABLE,       // Enable interrupt when timer resets
		TIMER_A_CCIE_CCR0_INTERRUPT_DISABLE, // Disable interrupt when timer == period - 1
        TIMER_A_SKIP_CLEAR                   // Skip Clear Counter
};

/* Timer_A Compare Configuration Parameter  (PWM1) */
Timer_A_CompareModeConfig compareConfig_PWM1 =
{
        TIMER_A_CAPTURECOMPARE_REGISTER_1,          // Use CCR1
        TIMER_A_CAPTURECOMPARE_INTERRUPT_DISABLE,   // Disable CCR interrupt
        TIMER_A_OUTPUTMODE_TOGGLE_SET,              // Toggle output but
        64                                          // 64 Duty Cycle
};

/* Timer_A Capture Mode Configuration Parameter */
// This isn't used
const Timer_A_CaptureModeConfig captureModeConfig =
{
        TIMER_A_CAPTURECOMPARE_REGISTER_1,        // CC Register 1
        TIMER_A_CAPTUREMODE_RISING_EDGE,          // Rising Edge
        TIMER_A_CAPTURE_INPUTSELECT_CCIxB,        // CCIxB Input Select
        TIMER_A_CAPTURE_SYNCHRONOUS,              // Synchronized Capture
        TIMER_A_CAPTURECOMPARE_INTERRUPT_ENABLE,  // Enable interrupt
        TIMER_A_OUTPUTMODE_OUTBITVALUE            // Output bit value
};

/* Interrupt Handler Routines */
void PORT2_IRQHandler(void);
void TA0_N_IRQHandler(void);

/* Statics */
#define NUMBER_TIMER_CAPTURES       20
static volatile uint_fast16_t timerAcaptureValues[NUMBER_TIMER_CAPTURES];
static volatile uint32_t timerAcapturePointer = 0;

static volatile uint_fast16_t count = 0;

int main(void)
{
    MAP_WDT_A_holdTimer();

    /* Remapping  TACCR0 to P2.4 */
    MAP_PMAP_configurePorts((const uint8_t *) port2_mapping, PMAP_P2MAP, 1,
            PMAP_ENABLE_RECONFIGURATION);

    /* Configuring P1.0 as output */
    // Don't think we need this line
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);

    /* Setting input and output pins */
    MAP_GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P2,
            GPIO_PIN4, GPIO_PRIMARY_MODULE_FUNCTION);

    GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN3);
	GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN3);  // Set direction pin as low

	MAP_GPIO_setAsInputPinWithPullDownResistor(GPIO_PORT_P2, GPIO_PIN6);

    /* Initialize compare registers to generate PWM1 */
    MAP_Timer_A_initCompare(TIMER_A1_BASE, &compareConfig_PWM1);

    /* Configuring Timer_A1 for UpDown Mode and starting */
    MAP_Timer_A_configureUpDownMode(TIMER_A1_BASE, &upDownConfig);
    MAP_Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_UPDOWN_MODE);

    // Not sure why we have these
    MAP_CS_setReferenceOscillatorFrequency(CS_REFO_128KHZ);
    MAP_CS_initClockSignal(CS_ACLK, CS_REFOCLK_SELECT, CS_CLOCK_DIVIDER_4);

    /* Configuring Timer_A0 for Up Mode and starting */
    MAP_Timer_A_configureUpMode(TIMER_A0_BASE, &upModeConfig);
    MAP_Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);

	/* Enabling interrupts */
	MAP_GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN6);
	MAP_GPIO_enableInterrupt(GPIO_PORT_P2, GPIO_PIN6);

	Interrupt_registerInterrupt(INT_PORT2, PORT2_IRQHandler);
	Interrupt_setPriority(INT_PORT2, 0x18); // Set encoder counter interrupt at higher priority
	Interrupt_registerInterrupt(INT_TA0_N, TA0_N_IRQHandler);

	MAP_Interrupt_enableInterrupt(INT_PORT2);
	MAP_Interrupt_enableInterrupt(INT_TA0_N);
	MAP_Interrupt_enableMaster();

    while (1);

}

//******************************************************************************
//
//This is the PORT2 interrupt vector service routine. Encoder counts are
//	accumulated here from Pins 2.5 and 2.6.
//
//******************************************************************************
void PORT2_IRQHandler(void)
{

	uint32_t status = MAP_GPIO_getEnabledInterruptStatus(GPIO_PORT_P2);

    if (GPIO_PIN5 & status)
    {
    	// Need to figure out how to count on both 2.5 and 2.6
    }
    if (GPIO_PIN6 & status)
    {
    	count++;
    }

    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P2,status);
}

//******************************************************************************
//
//This is the TIMERA0 interrupt vector service routine. PID Control is
//	implemented here.
//
//******************************************************************************

/* Setting up moving average buffer */
#define NUM_COUNTS 8
int count_buffer[NUM_COUNTS];
int buf_offset = 0;
int i, avg_count;

void TA0_N_IRQHandler(void)
{
	// This is a debug statement
	if(count % 180 == 0)
	  printf("Count: %d\n", count);

	count_buffer[buf_offset] = count;
	buf_offset = (buf_offset + 1) % NUM_COUNTS;

	// Calculate the speed using moving average
	for(i = 0; i < NUM_COUNTS; i++){
		avg_count += count_buffer[i];
	}
	avg_count = avg_count >> 3;

	// Calculate proportional control

	// Calculate integral control

	// Calculate derivative control
}
