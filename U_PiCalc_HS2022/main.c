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
void calculatEuler(void* pvParameters);
void ButtonTask(void *pvParameters);
float piGerechnet;

//EventGroup for ButtonEvents.
EventGroupHandle_t egButtonEvents = NULL;
#define BUTTON1_SHORT	0x01	// Startet den Algorithmus
#define BUTTON2_SHORT	0x02	// Stoppt den Algorithmus
#define BUTTON3_SHORT	0x04	// Setzt den Algorithmus zurück
#define BUTTON4_SHORT	0x08	// Wechseln vom Algorithmus
#define START_PI_1		0x10	// Start Stopp Bit für Leibnitz Pi Berechnung
#define START_PI_2		0x20	// Start Stopp Bit für Euler Pi berechnung
#define PI_READY		0x40	// Rückmeldung sobald Pi Task beendet und auf Stopp ist
#define PI_GENAU		0x80	// Pi Genauigkeit erreicht
#define BUTTON_ALL		0xFF	// Rücksetzten der Event Bit

int main(void)
{
	vInitClock();
	vInitDisplay();
	xEventGroupClearBits(egButtonEvents, BUTTON_ALL);
	
	xTaskCreate(controllerTask, (const char *) "control_tsk", configMINIMAL_STACK_SIZE+150, NULL, 2, NULL);			// Steuer Task
	xTaskCreate(ButtonTask, (const char *) "Button_task", configMINIMAL_STACK_SIZE+150, NULL, 3, NULL);				// Einlesen knöpfe
	xTaskCreate(calculatLeibniz, (const char *) "calculat_leibniz", configMINIMAL_STACK_SIZE+150, NULL, 1, NULL);	// Pi berechnen mit Leibniz
	xTaskCreate(calculatEuler, (const char *) "calculat_euler", configMINIMAL_STACK_SIZE+150, NULL, 1, NULL);		// Pi berechnen mit Euler

// 	vDisplayClear();
// 	vDisplayWriteStringAtPos(0,0,"PI-Calc HS2022 Philipp");
	vTaskStartScheduler();
	return 0;
}

//Modes for Finite State Machine
#define MODE_IDLE 0
#define MODE_PI_IN_AUSGANGS_VAR 1
#define MODE_DISPLAY_AKTUEALISIEREN 2

// Steuerungs Task das Display und die sichere ausgabe von Pi macht
void controllerTask(void* pvParameters) {
	float Pi_Ausgabe = 0;
	uint16_t zaehler = 0;		
	uint8_t mode = MODE_IDLE;
	while(egButtonEvents == NULL) { //Wait for EventGroup to be initialized in other task
		vTaskDelay(10/portTICK_RATE_MS);
	}
	vDisplayClear();
	vDisplayWriteStringAtPos(0,0,"Pi Rechner V1.0");
	
	for(;;) {
		switch(mode) {
			case MODE_IDLE: {		
				if (xEventGroupGetBits(egButtonEvents) & BUTTON1_SHORT){
					xEventGroupSetBits(egButtonEvents, START_PI_1);
				}
				if (xEventGroupGetBits(egButtonEvents) & BUTTON2_SHORT){
					xEventGroupClearBits(egButtonEvents, START_PI_1);
				}
				
				if (zaehler >= 200 || zaehler >= 400 || zaehler >= 600){
					mode = MODE_DISPLAY_AKTUEALISIEREN;
					if (zaehler >= 600){
						zaehler = 0;
					}
				}
				if (zaehler == 500){
					mode = MODE_PI_IN_AUSGANGS_VAR;
				}
				zaehler = zaehler+1;
				break;	
			}
			case MODE_PI_IN_AUSGANGS_VAR: {
				uint8_t zustand = 0;
				if (xEventGroupGetBits(egButtonEvents) & START_PI_1){
					
					xEventGroupClearBits(egButtonEvents, START_PI_1);
					xEventGroupClearBits(egButtonEvents, START_PI_2);
					zustand = 0;
				}
				else{
					xEventGroupClearBits(egButtonEvents, START_PI_1);
					xEventGroupClearBits(egButtonEvents, START_PI_2);
					zustand = 1;
				}
				xEventGroupWaitBits(egButtonEvents, PI_READY, false, true, portMAX_DELAY);
				
				Pi_Ausgabe = piGerechnet;
				
				if ( zustand == 0){
					xEventGroupSetBits(egButtonEvents, START_PI_1);
					xEventGroupClearBits(egButtonEvents, PI_READY);
				}
				else{
					xEventGroupSetBits(egButtonEvents, START_PI_2);
					xEventGroupClearBits(egButtonEvents, PI_READY);
				}
				mode = MODE_IDLE;
			
				break;
			}
			case MODE_DISPLAY_AKTUEALISIEREN: {
				
				if(xEventGroupGetBits(egButtonEvents) & START_PI_1){
					vDisplayClear();
					vDisplayWriteStringAtPos(0,0,"Pi wird mit Leibniz berechnet");
					vDisplayWriteStringAtPos(1,0,"Aktueller Wert: %d",Pi_Ausgabe);
					vDisplayWriteStringAtPos(2,0,"Zeit seit beginn: ");
					vDisplayWriteStringAtPos(3,0,"B1 str | B2 stp | B3 rst | B4 Wechsel");
				}
				else if(xEventGroupGetBits(egButtonEvents) & START_PI_2){
					vDisplayClear();
					vDisplayWriteStringAtPos(0,0,"Pi wird mit Euler berechnet");
					vDisplayWriteStringAtPos(1,0,"Aktueller Wert: %d",Pi_Ausgabe);
					vDisplayWriteStringAtPos(2,0,"Zeit seit beginn: ");
					vDisplayWriteStringAtPos(3,0,"B1 str | B2 stp | B3 rst | B4 Wechsel");
				}
				else{
					vDisplayClear();
					vDisplayWriteStringAtPos(0,0,"Zum Pi Rechnen Start Drücken");
					vDisplayWriteStringAtPos(1,0,"1");
					vDisplayWriteStringAtPos(2,0,"1");
					vDisplayWriteStringAtPos(3,0,"B1 str | B2 stp | B3 rst | B4 Wechsel");
				}
				mode = MODE_IDLE;
				break;
			}
		}
		vTaskDelay(10/portTICK_RATE_MS);
		
	}
}

// Pi berechnen mit der Leibnitz Reihe
void calculatLeibniz(void* pvParameters){
	float Pi4_L = 1.0;
	if (xEventGroupGetBits(egButtonEvents) & START_PI_1){
		for(;;){
			uint32_t n =3;
			Pi4_L = Pi4_L -(1.0/n)+(1.0/(n+2));
			n = n+4;
			piGerechnet = Pi4_L*4;
		}
	}
	else{
		xEventGroupSetBits(egButtonEvents, PI_READY);
		xEventGroupWaitBits(egButtonEvents, START_PI_1, false, true, portMAX_DELAY);
	}
}

// Berechnen von Pi mit der Euler Reihe
void calculatEuler(void *pvParameters){
	float PiEuler = 0;
	
	if (xEventGroupGetBits(egButtonEvents) & START_PI_2){
		for(;;){
			uint32_t n = 1;
			PiEuler = PiEuler + (1/(n^2));
			n++;
			piGerechnet = sqrt(PiEuler*6);
		}
	}
	else{
		xEventGroupSetBits(egButtonEvents, PI_READY);
		xEventGroupWaitBits(egButtonEvents, START_PI_2, false, true, portMAX_DELAY);
	}
}

// Task liest die Buttons ein uns setzt die eventbits
void ButtonTask(void *pvParameters) {
	egButtonEvents = xEventGroupCreate();
	initButtons(); //Initialize Buttons
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
		vTaskDelay((1000/BUTTON_UPDATE_FREQUENCY_HZ)/portTICK_RATE_MS);
	}
}