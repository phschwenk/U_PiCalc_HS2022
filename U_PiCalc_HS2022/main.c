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
#define BUTTON_ALL		0x0F	// Rücksetzten der Event Bit

int main(void){
	//int start =1;
	vInitClock();
	vInitDisplay();
// 	if (start == 1){
// 		vDisplayClear();
// 		vDisplayWriteStringAtPos(0,0,"PI-Calc HS2022");
// 		vDisplayWriteStringAtPos(1,0,"Philipp Schwenk");
// 		start = 0;
// 	}
	
	xTaskCreate(controllerTask, (const char *) "control_tsk", configMINIMAL_STACK_SIZE+150, NULL, 2, NULL);			// Steuer Task
	xTaskCreate(ButtonTask, (const char *) "Button_task", configMINIMAL_STACK_SIZE+150, NULL, 3, NULL);				// Einlesen knöpfe
	xTaskCreate(calculatLeibniz, (const char *) "calculat_leibniz", configMINIMAL_STACK_SIZE+150, NULL, 1, NULL);	// Pi berechnen mit Leibniz
	xTaskCreate(calculatEuler, (const char *) "calculat_euler", configMINIMAL_STACK_SIZE+150, NULL, 1, NULL);		// Pi berechnen mit Euler

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
	char PI_STRING[10]; //Variable for temporary string
	uint16_t zaehler = 0;
	uint8_t zustand =0;		
	uint8_t mode = MODE_IDLE;
	while(egButtonEvents == NULL) { //Wait for EventGroup to be initialized in other task
		vTaskDelay(10/portTICK_RATE_MS);
	}

	
	for(;;) {
		switch(mode) {
			case MODE_IDLE: {
				zaehler = zaehler+1;
				//xEventGroupSetBits(egButtonEvents, START_PI_1);		
				if (xEventGroupGetBits(egButtonEvents) & BUTTON1_SHORT){
					if (zustand == 0){
						xEventGroupSetBits(egButtonEvents, START_PI_1);
					}
					else{
						xEventGroupSetBits(egButtonEvents, START_PI_2);
					}
				}
				if (xEventGroupGetBits(egButtonEvents) & BUTTON2_SHORT){
					if (zustand == 0){
						xEventGroupClearBits(egButtonEvents, START_PI_1);
					}
					else{
							xEventGroupClearBits(egButtonEvents, START_PI_2);
					}
				}
				if (xEventGroupGetBits(egButtonEvents)& BUTTON4_SHORT){
					if (zustand==0){
						zustand =1;
						xEventGroupClearBits(egButtonEvents,START_PI_1);
					}
					else if (zustand ==1){
						xEventGroupClearBits(egButtonEvents,START_PI_2);
						zustand =0;
					}
				}
				
				if (zaehler == 20 || zaehler == 40 || zaehler == 60){
					if (zaehler == 60){
						zaehler = 0;
						mode = MODE_DISPLAY_AKTUEALISIEREN;
					}
					else{
						mode = MODE_DISPLAY_AKTUEALISIEREN;
					}
				}
				if (zaehler == 50){
					mode = MODE_PI_IN_AUSGANGS_VAR;
				}
				
				break;	
			}
			case MODE_PI_IN_AUSGANGS_VAR: {
				uint8_t zustand = 2;
				if (xEventGroupGetBits(egButtonEvents) & START_PI_1){
					
					xEventGroupClearBits(egButtonEvents, START_PI_1);
					zustand = 0;
				}
				else if (xEventGroupGetBits(egButtonEvents) & START_PI_2){
					xEventGroupClearBits(egButtonEvents, START_PI_2);
					zustand = 1;
				}

				xEventGroupWaitBits(egButtonEvents, PI_READY, false, false, portMAX_DELAY);
				
				Pi_Ausgabe = piGerechnet;
				
				if ( zustand == 0){
					xEventGroupSetBits(egButtonEvents, START_PI_1);
					xEventGroupClearBits(egButtonEvents, PI_READY);
				}
				else if ( zustand == 1){
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
					sprintf(PI_STRING, "%1.5f",Pi_Ausgabe); //Writing Time into one string
					vDisplayWriteStringAtPos(1,0,"Wert: %s", PI_STRING);
					vDisplayWriteStringAtPos(2,0,"Zeit seit begin: ");
					vDisplayWriteStringAtPos(3,0,"str|stp|rst|Wechsel");
				}
				else if(xEventGroupGetBits(egButtonEvents) & START_PI_2){
					vDisplayClear();
					vDisplayWriteStringAtPos(0,0,"Pi mit Euler berechnet");
					sprintf(PI_STRING, "%1.5f",Pi_Ausgabe); //Writing Time into one string
					vDisplayWriteStringAtPos(1,0,"Wert: %s",PI_STRING);
					vDisplayWriteStringAtPos(2,0,"Zeit seit begin: ");
					vDisplayWriteStringAtPos(3,0,"str|stp|rst|Wechsel");
				}
				else{
					vDisplayClear();
					vDisplayWriteStringAtPos(0,0,"Zum Pi Rechnen Start Drücken");
					vDisplayWriteStringAtPos(1,0,"1");
					vDisplayWriteStringAtPos(2,0,"1");
					vDisplayWriteStringAtPos(3,0,"str|stp|rst|Wechsel");
				}
				mode = MODE_IDLE;
				
				break;
			}
		}
		xEventGroupClearBits(egButtonEvents, BUTTON_ALL);
		vTaskDelay(10/portTICK_RATE_MS);
		
	}
}

// Pi berechnen mit der Leibnitz Reihe
void calculatLeibniz(void* pvParameters){
	float Pi4_L = 1.0;
	uint32_t n = 3;
	char pi_string_1[10] = "3.14159";
	char pi_string_2[10] = "0";
	
	while(egButtonEvents == NULL) { //Wait for EventGroup to be initialized in other task
		vTaskDelay(10/portTICK_RATE_MS);
	}
	
	for(;;){
		xEventGroupSetBits(egButtonEvents, PI_READY);
		
		if (xEventGroupGetBits(egButtonEvents)&START_PI_1){
	
			if (pi_string_1 != pi_string_2){
				while (xEventGroupGetBits(egButtonEvents)&START_PI_1){
					Pi4_L = Pi4_L -(1.0/n)+(1.0/(n+2));
					n = n+4;
					piGerechnet = Pi4_L*4;
					sprintf(pi_string_2,"%1.5f",piGerechnet);
				}
			}
		}
		else{
			if (xEventGroupGetBits(egButtonEvents)&BUTTON3_SHORT){
				Pi4_L = 1.0;
				n = 3;
			}
		}
	}
}

// Berechnen von Pi mit der Euler Reihe
void calculatEuler(void *pvParameters){
	float PiEuler = 0;
	uint32_t n = 1;
	char pi_string_1[10] = "3.14159";
	char pi_string_2[10] = "0";
	
	while(egButtonEvents == NULL) { //Wait for EventGroup to be initialized in other task
		vTaskDelay(10/portTICK_RATE_MS);
	}
	
	for(;;){
		xEventGroupSetBits(egButtonEvents, PI_READY);
		xEventGroupWaitBits(egButtonEvents, START_PI_2, false, false, portMAX_DELAY);
		
		if (pi_string_1 != pi_string_2){
			while (xEventGroupGetBits(egButtonEvents) & START_PI_2){
		
			
					PiEuler = PiEuler + (1.0/(pow(n,2)));
					n++;
					piGerechnet = sqrt(PiEuler*6);
					sprintf(pi_string_2,"%1.5f",piGerechnet);
			}
		}
		else{
			if (xEventGroupGetBits(egButtonEvents)&BUTTON3_SHORT){
				PiEuler = 0.0;
				n = 1;
			}
		}
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