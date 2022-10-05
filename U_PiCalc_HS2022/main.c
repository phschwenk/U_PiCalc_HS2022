/*
 * U_PiCalc_HS2022.c
 *
 * Created: 05.10.2022 19:00
 * Author : Philipp Schwenk
 */ 

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "avr_compiler.h"
#include "pmic_driver.h"
#include "TC_driver.h"
#include "clksys_driver.h"
#include "sleepConfig.h"
#include "port_driver.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "stack_macros.h"

#include "mem_check.h"

#include "init.h"
#include "utils.h"
#include "errorHandler.h"
#include "NHD0420Driver.h"

#include "ButtonHandler.h"


void controllerTask(void* pvParameters);
void calculatLeibniz(void* pvParameters);
float piGerechnet;

//EventGroup for ButtonEvents.
EventGroupHandle_t egButtonEvents = NULL;
#define BUTTON1_SHORT	0x01	// Startet den Algorithmus
#define BUTTON2_SHORT	0x02	// Stoppt den Algorithmus
#define BUTTON3_SHORT	0x04	// Setzt den Algorithmus zurück
#define BUTTON4_SHORT	0x08	// Für Zustand und Wechseln vom Algorithmus
#define BUTTON_ALL		0xFF	// Rücksetzten der event Bit

int main(void)
{
	vInitClock();
	vInitDisplay();
	
	xTaskCreate( controllerTask, (const char *) "control_tsk", configMINIMAL_STACK_SIZE+150, NULL, 3, NULL);
	xTaskCreate(calculatLeibniz, (const char *) "calculat_leibniz", configMINIMAL_STACK_SIZE+150, NULL, 1, NULL)

	vDisplayClear();
	vDisplayWriteStringAtPos(0,0,"PI-Calc HS2022");
	
	vTaskStartScheduler();
	return 0;
}

//Modes for Finite State Machine
#define MODE_IDLE 0
#define MODE_Leibnitz 1
#define MODE_KommtNoch 2
#define MODE_ 3

void controllerTask(void* pvParameters) {
	egButtonEvents = xEventGroupCreate();
	initButtons();
	for(;;) {
		updateButtons();
		if(getButtonPress(BUTTON1) == SHORT_PRESSED) {
			char pistring[12];
			sprintf(&pistring[0], "PI: %.8f", piGerechnet);
			vDisplayWriteStringAtPos(1,0, "%s", pistring);
	}
	for(;;) {
		updateButtons();
			
		if(getButtonPress(BUTTON1) == SHORT_PRESSED) {
			xEventGroupSetBits(egButtonEvents, BUTTON1_SHORT);
		}
		if(getButtonPress(BUTTON2) == SHORT_PRESSED) {
			xEventGroupSetBits(egButtonEvents, BUTTON2_SHORT);
		}
		if(getButtonPress(BUTTON3) == SHORT_PRESSED) {
			xEventGroupSetBits(egButtonEvents, BUTTON3_SHORT);
		}
		if(getButtonPress(BUTTON4) == SHORT_PRESSED) {
			xEventGroupSetBits(egButtonEvents, BUTTON4_SHORT);
		}
	vTaskDelay(10/portTICK_RATE_MS);
	}
}
void calculatLeibniz(void* pvParameters){
	float Pi4_L = 1.0;
	for(;;){
		uint32_t n =3;
		Pi4_L = Pi4_L -(1.0/n)+(1.0/(n+2));
		n = n+4;
		piGerechnet = Pi4_L*4;
	}
}