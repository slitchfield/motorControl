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
 * Description: This program generates a PWM output on P2.4 using the port
 * mapper to internally redirect the CCR0 output of Timer A1 to P2.4 (it
 * is originally P7.7). After the port mapping function is called, the timer
 * is setup normally with a 75% duty cycle. The output of the timer can be
 * seen on P2.4 using a probe/debugger.
 *
 *         MSP432P401
 *      -------------------
 *  /|\|                   |
 *   | |                   |
 *   --|RST                |
 *     |                   |
 *     |             P2.4  |--> CCR1 - 75% PWM
 *     |                   |
 *
 * Author: Timothy Logan
*******************************************************************************/
/* DriverLib Includes */
#include "driverlib.h"

/* Standard Includes */
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define NUMBER_TIMER_CAPTURES       25

/* Port mapper configuration register */
const uint8_t port2_mapping[] =
{
        //Port P2:
        PM_NONE, PM_NONE, PM_NONE, PM_NONE, PM_TA1CCR1A, PM_NONE, PM_NONE,
        PM_NONE
};

const uint8_t port3_mapping[] =
{
		//Port P3:
		PM_NONE, PM_NONE, PM_NONE, PM_NONE, PM_NONE, PM_TA0CCR1A, PM_NONE, PM_NONE
};

/* Timer_A UpDown Configuration Parameter */
const Timer_A_UpDownModeConfig upDownConfig =
{
        TIMER_A_CLOCKSOURCE_SMCLK,              // SMCLK Clock SOurce
        TIMER_A_CLOCKSOURCE_DIVIDER_8,          // SMCLK/1 = 3MHz
        127,                                    // 127 tick period
        TIMER_A_TAIE_INTERRUPT_DISABLE,         // Disable Timer interrupt
        TIMER_A_CCIE_CCR0_INTERRUPT_DISABLE,    // Disable CCR0 interrupt
        TIMER_A_DO_CLEAR                        // Clear value

};

/* Timer_A Compare Configuration Parameter  (PWM1) */
Timer_A_CompareModeConfig compareConfig_PWM1 =
{
        TIMER_A_CAPTURECOMPARE_REGISTER_1,          // Use CCR1
        TIMER_A_CAPTURECOMPARE_INTERRUPT_DISABLE,   // Disable CCR interrupt
        TIMER_A_OUTPUTMODE_TOGGLE_SET,              // Toggle output but
        64                                          // 32 Duty Cycle
};

/* Timer_A Capture Mode Configuration Parameter */
const Timer_A_CaptureModeConfig captureModeConfig =
{
        TIMER_A_CAPTURECOMPARE_REGISTER_1,        // CC Register 2
        TIMER_A_CAPTUREMODE_RISING_EDGE,          // Rising Edge
        TIMER_A_CAPTURE_INPUTSELECT_CCIxB,        // CCIxB Input Select
        TIMER_A_CAPTURE_SYNCHRONOUS,              // Synchronized Capture
        TIMER_A_CAPTURECOMPARE_INTERRUPT_ENABLE,  // Enable interrupt
        TIMER_A_OUTPUTMODE_OUTBITVALUE            // Output bit value
};

/* Timer_A Continuous Mode Configuration Parameter */
const Timer_A_ContinuousModeConfig continuousModeConfig =
{
        TIMER_A_CLOCKSOURCE_ACLK,           // SMCLK Clock Source
        TIMER_A_CLOCKSOURCE_DIVIDER_3,       // SMCLK/1 = 3MHz
        TIMER_A_TAIE_INTERRUPT_ENABLE,       // Disable Timer ISR
        TIMER_A_SKIP_CLEAR                   // Skup Clear Counter
};

const Timer_A_UpModeConfig upModeConfig =
{
        TIMER_A_CLOCKSOURCE_ACLK,           // ACLK Clock Source
        TIMER_A_CLOCKSOURCE_DIVIDER_1,       // ACLK/1 = 32.768KHz
		//TIMER_A_CLOCKSOURCE_DIVIDER_64,
		327,								 // Timer Period -> Should step down to 100hz
        TIMER_A_TAIE_INTERRUPT_ENABLE,       // Enable interrupt when timer resets
		TIMER_A_CCIE_CCR0_INTERRUPT_DISABLE, // Disable interrupt when timer == period - 1
        TIMER_A_SKIP_CLEAR                   // Skup Clear Counter
};

void PORT2_IRQHandler(void);
void TA0_N_IRQHandler(void);

/* Statics */
static volatile uint_fast16_t timerAcaptureValues[NUMBER_TIMER_CAPTURES];
static volatile uint32_t timerAcapturePointer = 0;

static volatile uint_fast16_t count = 0;

int main(void)
{
    MAP_WDT_A_holdTimer();

    /* Remapping  TACCR0 to P2.4 */
    MAP_PMAP_configurePorts((const uint8_t *) port2_mapping, PMAP_P2MAP, 1,
            PMAP_ENABLE_RECONFIGURATION);
    //MAP_PMAP_configurePorts((const uint8_t *) port3_mapping, PMAP_P3MAP, 1,
    //        PMAP_DISABLE_RECONFIGURATION);

    /* Configuring P1.0 as output */
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);

    // Set input and output pins
    MAP_GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P2,
            GPIO_PIN4, GPIO_PRIMARY_MODULE_FUNCTION);
    //MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P3, GPIO_PIN5,
    //            GPIO_PRIMARY_MODULE_FUNCTION);


    /* Initialize compare registers to generate PWM1 */
    MAP_Timer_A_initCompare(TIMER_A1_BASE, &compareConfig_PWM1);

    /* Configuring Timer_A1 for UpDown Mode and starting */
    MAP_Timer_A_configureUpDownMode(TIMER_A1_BASE, &upDownConfig);
    MAP_Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_UPDOWN_MODE);

    /* Configuring Capture Mode */
    //MAP_Timer_A_initCapture(TIMER_A0_BASE, &captureModeConfig);

    /* Configuring Continuous Mode */
    //MAP_Timer_A_configureContinuousMode(TIMER_A0_BASE, &continuousModeConfig);

    MAP_CS_setReferenceOscillatorFrequency(CS_REFO_128KHZ);
    MAP_CS_initClockSignal(CS_ACLK, CS_REFOCLK_SELECT, CS_CLOCK_DIVIDER_4);

    MAP_Timer_A_configureUpMode(TIMER_A0_BASE, &upModeConfig);

    /* Enabling interrupts and going to sleep */
    //MAP_Interrupt_enableSleepOnIsrExit();
    //MAP_Interrupt_enableInterrupt(INT_TA0_N);


    /* Starting the Timer_A0 in continuous mode */
    MAP_Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);

    /* Set direction pin as low */
    GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN3);
	GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN3);

	MAP_GPIO_setAsInputPinWithPullDownResistor(GPIO_PORT_P2, GPIO_PIN6);
	MAP_GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN6);
	MAP_GPIO_enableInterrupt(GPIO_PORT_P2, GPIO_PIN6);

	Interrupt_registerInterrupt(INT_PORT2, PORT2_IRQHandler);
	Interrupt_setPriority(INT_PORT2, 0x18);
	Interrupt_registerInterrupt(INT_TA0_N, TA0_N_IRQHandler);

	MAP_Interrupt_enableInterrupt(INT_PORT2);
	MAP_Interrupt_enableInterrupt(INT_TA0_N);
	MAP_Interrupt_enableMaster();

    while (1);

}

/* GPIO ISR */
void PORT2_IRQHandler(void)
{
	//printf("Interrupt!\n");
	uint32_t status = MAP_GPIO_getEnabledInterruptStatus(GPIO_PORT_P2);

    if (GPIO_PIN5 & status)
    {
    	//printf("Interrupt!\n");
        //MAP_GPIO_disableInterrupt(GPIO_PORT_P3, GPIO_PIN5);
        // Store the last 20 timer captures
	    //timerAcaptureValues[timerAcapturePointer] =
	    //        MAP_Timer_A_getCounterValue(TIMER_A0_BASE);

	    // Make the buffer circular
	    //timerAcapturePointer = (timerAcapturePointer + 1) % NUMBER_TIMER_CAPTURES;
    	//count++;
    }
    if (GPIO_PIN6 & status)
    {
    	//printf("Got stuff we don't care about yet\n");
    	count++;
    }

    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P2,status);
}

//******************************************************************************
//
//This is the TIMERA interrupt vector service routine.
//
//******************************************************************************

#define NUM_COUNTS 8
int count_buffer[NUM_COUNTS];
int buf_offset = 0;
int i, avg_count;

void TA0_N_IRQHandler(void)
{
	if(count % 180 == 0)
	  printf("Count: %d\n", count);

	count_buffer[buf_offset] = count;
	buf_offset = (buf_offset + 1) % NUM_COUNTS;

	// Calculate the speed!
	for(i = 0; i < NUM_COUNTS; i++){
		avg_count += count_buffer[i];
	}
	avg_count = avg_count >> 3;

	// Calculate proportional control

	// Calculate integral control

	// Calculate derivative control
}
