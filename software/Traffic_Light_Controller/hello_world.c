#include <stdio.h>
#include "sys/alt_alarm.h"
#include <system.h>
#include <altera_avalon_pio_regs.h>
#include <math.h>
#include <string.h>
#include <alt_types.h> //alt_u32 is a kind of alt_type
#include <sys/alt_irq.h> //to register interrupts

//define timer timeout values for mode1/2/4
#define rrrr 500
#define grrg 6000
#define yrry 2000
#define camera_timeout 2000

//Mode 1 states
enum simple_state{rr1_1,gr_1,yr_1,rr2_1,rg_1,ry_1,buffer_1};

//Mode 2 states
enum pedestrian_state{rr1_2,gr_2,grp1_2,yr_2,rr2_2,rg_2,rgp2_2,ry_2,buffer_2};

//Mode 3 states
enum configurable_state{rr1_3,gr_3,grp1_3,yr_3,rr2_3,rg_3,rgp2_3,ry_3,buffer_3};

//Mode 4 states
enum camera_state{rr1_4,gr_4,grp1_4,yr_4,rr2_4,rg_4,rgp2_4,ry_4,buffer_4};

//global variable declaration section for peripheral streams, states, flags, and constants
volatile alt_alarm timer_simple;
volatile alt_alarm timer_camera;
volatile alt_alarm timer_vehicle;
volatile FILE *lcd;
volatile FILE *uart;
volatile int timer_has_started = 0;
volatile int camera_timer_has_started = 0;
volatile int current_mode = 50;
volatile int mode_request = 0;
volatile int mode_display = 50;
volatile char config_values[50];
volatile enum simple_state current_state1 = rr1_1;
volatile enum simple_state previous_state1 = rr1_1;
volatile enum pedestrian_state current_state2 = rr1_2;
volatile enum pedestrian_state previous_state2 = rr1_2;
volatile enum configurable_state current_state3 = rr1_3;
volatile enum configurable_state previous_state3 = rr1_3;
volatile enum camera_state current_state4 = rr1_4;
volatile enum camera_state previous_state4 = rr1_4;
volatile int pedNS = 0;
volatile int pedEW = 0;
volatile int vehicle_cross = 0;
volatile unsigned long int count = 0; //vehicle timer counter
const char comma[2] = ",";
//create timer variables for mode 3, and initialise them to the default preset values
volatile unsigned int t1 = rrrr;
volatile unsigned int t2 = grrg;
volatile unsigned int t3 = yrry;
volatile unsigned int t4 = rrrr;
volatile unsigned int t5 = grrg;
volatile unsigned int t6 = yrry;

//helper function to reset volatile variables on mode change
void reset_volatiles(){
	current_state1 = 0;
	current_state2 = 0;
	current_state3 = 0;
	current_state4 = 0;
	previous_state1 = 0;
	previous_state2 = 0;
	previous_state3 = 0;
	previous_state4 = 0;
	pedNS = 0;
	pedEW = 0;
	vehicle_cross = 0;
	count = 0;
	t1 = rrrr;
	t2 = grrg;
	t3 = yrry;
	t4 = rrrr;
	t5 = grrg;
	t6 = yrry;
}

alt_u32 simple_tlc_timer_isr(void* context){
	//if there is a pending mode request and we are in a safe state, do not run any output logic and kill the timer
	if (mode_request != current_mode){
		if ((current_state1 == rr1_1) || (current_state1 == rr2_1))
			return 0;
	}

	previous_state1 = current_state1; //save previous state for output transition logic
	current_state1++; //move to the next state
	if (current_state1 == buffer_1) //if at final state, loop back to initial state using a buffer state
		current_state1 = rr1_1;
	printf("current state simple_timer %d\n", current_state1);
	timer_has_started = 0; //reset timer_started flag
	return 0;
}

alt_u32 pedestrian_tlc_timer_isr(void* context){
	//if there is a pending mode request and we are in a safe state, do not run any output logic and kill the timer
	if (mode_request != current_mode){
		if ((current_state2 == rr1_2) | (current_state2 == rr2_2))
			return 0;
	}

	previous_state2 = current_state2; //save previous state for output transition logic
	if ((pedEW) && (pedNS)){
		//handle pedestrians at either of the RED-RED states because
		if ((current_state2 == rr1_2) | (current_state2 == rr2_2))
			current_state2 += 2;
		//we are at state GREEN-RED, too late to handle NS pedestrians so skip past PED state
		else if (current_state2 == gr_2)
			current_state2 += 2;
		//we are at state RED-GREEN, cannot handle NS pedestrians here so skip past PED state
		else if (current_state2 == rg_2)
			current_state2 += 2;
		else
			current_state2++;
	}
	else if (pedNS){
		//we are at state RED-RED (1), handle NS pedestrians
		if (current_state2 == rr1_2)
			current_state2 += 2;
		//we are at state GREEN-RED, too late to handle NS pedestrians so skip past PED state
		else if (current_state2 == gr_2)
			current_state2 += 2;
		//we are at state RED-GREEN, cannot handle NS pedestrians here so skip past PED state
		else if (current_state2 == rg_2)
			current_state2 += 2;
		else
			current_state2++;
	}
	else if (pedEW){
		//we are at state RED-RED (2), handle EW pedestrians
		if (current_state2 == rr2_2)
			current_state2 += 2;
		//we are at state RED-GREEN, too late to handle EW pedestrians so skip past PED state
		else if (current_state2 == rg_2)
			current_state2 += 2;
		//we are at state GREEN-RED, cannot handle EW pedestrians here so skip past PED state
		else if (current_state2 == gr_2)
			current_state2 += 2;
		else
			current_state2++;
	}
	//if no PED interrupt flags, skip past PED states from GREEN-RED and RED-GREEN
	else if (current_state2 == gr_2)
		current_state2 += 2;
	else if (current_state2 == rg_2)
		current_state2 += 2;
	else
		current_state2++; //move to the next state if no special condition has been met

	//reset the NS/EW pedestrian button interrupt flag when we have changed state (NS/EWHandled = 1)
	if (previous_state2 == grp1_2)
		pedNS = 0;
	else if (previous_state2 == rgp2_2)
		pedEW = 0;

	if (current_state2 == buffer_2) //if at final state, loop back to initial state using buffer state
		current_state2 = rr1_2;
	printf("current state ped_timer %d\n", current_state2);

	timer_has_started = 0; //reset timer_started flag
return 0;
}

//timer ISR (and system state transition logic) for configurable timer
alt_u32 configurable_tlc_timer_isr(void* context){
	//if there is a pending mode request and we are in a safe state, do not run any output logic and kill the timer
	if (mode_request != current_mode){
		if ((current_state3 == rr1_3) || (current_state3 == rr2_3))
			return 0;
	}

	previous_state3 = current_state3; //save previous state for output transition logic
	if ((pedEW) && (pedNS)){
		//handle pedestrians at either of the RED-RED states because
		if ((current_state3 == rr1_3) | (current_state3 == rr2_3))
			current_state3 += 2;
		//we are at state GREEN-RED, too late to handle NS pedestrians so skip past PED state
		else if (current_state3 == gr_3)
			current_state3 += 2;
		//we are at state RED-GREEN, cannot handle NS pedestrians here so skip past PED state
		else if (current_state3 == rg_3)
			current_state3 += 2;
		else
			current_state3++;
	}
	else if (pedNS){
		//we are at state RED-RED (1), handle NS pedestrians
		if (current_state3 == rr1_3)
			current_state3 += 2;
		//we are at state GREEN-RED, too late to handle NS pedestrians so skip past PED state
		else if (current_state3 == gr_3)
			current_state3 += 2;
		//we are at state RED-GREEN, cannot handle NS pedestrians here so skip past PED state
		else if (current_state3 == rg_3)
			current_state3 += 2;
		else
			current_state3++;
	}
	else if (pedEW){
		//we are at state RED-RED (2), handle EW pedestrians
		if (current_state3 == rr2_3)
			current_state3 += 2;
		//we are at state RED-GREEN, too late to handle EW pedestrians so skip past PED state
		else if (current_state3 == rg_3)
			current_state3 += 2;
		//we are at state GREEN-RED, cannot handle EW pedestrians here so skip past PED state
		else if (current_state3 == gr_3)
			current_state3 += 2;
		else
			current_state3++;
	}
	//if no PED interrupt flags, skip past PED states from GREEN-RED and RED-GREEN
	else if (current_state3 == gr_3)
		current_state3 += 2;
	else if (current_state3 == rg_3)
		current_state3 += 2;
	else
		current_state3++; //move to the next state if no

	//reset the NS/EW pedestrian button interrupt flag when we have changed state (NS/EWHandled = 1)
	if (previous_state3 == grp1_3)
		pedNS = 0;
	else if (previous_state3 == rgp2_3)
		pedEW = 0;

	if (current_state3 == buffer_3) //if at final state, loop back to initial state using buffer state
		current_state3 = rr1_3;
	printf("current state config_timer %d\n", current_state3);

	timer_has_started = 0; //reset timer_started flag
return 0;
}

//timer ISR (and system state transition logic) for configurable timer
alt_u32 camera_tlc_timer_isr(void* context){
	//if there is a pending mode request and we are in a safe state, do not run any output logic and kill the timer
	if (mode_request != current_mode){
		if ((current_state4 == rr1_4) || (current_state4 == rr2_4))
			return 0;
	}

	previous_state4 = current_state4; //save previous state for output transition logic
	if ((pedEW) && (pedNS)){
		//handle pedestrians at either of the RED-RED states because
		if ((current_state4 == rr1_4) | (current_state4 == rr2_4))
			current_state4 += 2;
		//we are at state GREEN-RED, too late to handle NS pedestrians so skip past PED state
		else if (current_state4 == gr_4)
			current_state4 += 2;
		//we are at state RED-GREEN, cannot handle NS pedestrians here so skip past PED state
		else if (current_state4 == rg_4)
			current_state4 += 2;
		else
			current_state4++;
	}
	else if (pedNS){
		//we are at state RED-RED (1), handle NS pedestrians
		if (current_state4 == rr1_4)
			current_state4 += 2;
		//we are at state GREEN-RED, too late to handle NS pedestrians so skip past PED state
		else if (current_state4 == gr_4)
			current_state4 += 2;
		//we are at state RED-GREEN, cannot handle NS pedestrians here so skip past PED state
		else if (current_state4 == rg_4)
			current_state4 += 2;
		else
			current_state4++;
	}
	else if (pedEW){
		//we are at state RED-RED (2), handle EW pedestrians
		if (current_state4 == rr2_4)
			current_state4 += 2;
		//we are at state RED-GREEN, too late to handle EW pedestrians so skip past PED state
		else if (current_state4 == rg_4)
			current_state4 += 2;
		//we are at state GREEN-RED, cannot handle EW pedestrians here so skip past PED state
		else if (current_state4 == gr_4)
			current_state4 += 2;
		else
			current_state4++;
	}
	//if no PED interrupt flags, skip past PED states from GREEN-RED and RED-GREEN
	else if (current_state4 == gr_4)
		current_state4 += 2;
	else if (current_state4 == rg_4)
		current_state4 += 2;
	else
		current_state4++; //move to the next state if no

	//reset the NS/EW pedestrian button interrupt flag when we have changed state (NS/EWHandled = 1)
	if (previous_state4 == grp1_4) {
		pedNS = 0;
	}
	else if (previous_state4 == rgp2_4) {
		pedEW = 0;
	}

	if (current_state4 == buffer_4) //if at final state, loop back to initial state using buffer state
		current_state4 = rr1_4;
	printf("current state camera_timer %d\n", current_state4);

	timer_has_started = 0; //reset timer_started flag
return 0;
}

alt_u32 camera_timer_isr(void* context){
	fprintf(uart,"Snapshot taken\r\n");
	camera_timer_has_started = 0;
	return 0;
}

alt_u32 vehicle_timer_isr(void* context){
	count++;
	return 1;
}

//button interrupt function for pedestrian buttons and
void button_interrupt(void* context, alt_u32 id) {
	//read edge capture register and button value
	unsigned int edgeCapture = IORD_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE);
	unsigned int uiButtonsValue = IORD_ALTERA_AVALON_PIO_DATA(BUTTONS_BASE);
	void* timerContext = 0;

	//only allows the pedestrian inputs in mode 2,3 and 4
	if ((!(uiButtonsValue & 1<<0)) && !((current_mode == 1) || (current_mode == 0)))
		pedNS = 1;
	if ((!(uiButtonsValue & 1<<1)) && !((current_mode == 1) || (current_mode == 0)))
		pedEW = 1;
	//vehicle crossing interrupt
	if (current_mode == 4){
		if ((edgeCapture & 1<<2)) {
			++vehicle_cross;
			if (vehicle_cross % 2){
				alt_alarm_start(&timer_vehicle,1,vehicle_timer_isr,timerContext);
				if ((current_state4 == yr_4) || (current_state4 == ry_4)){
					//prevent crashes by checking if timer has already started
					if (!camera_timer_has_started){
						fprintf(uart,"Camera activated\r\n");
						camera_timer_has_started = 1;
						alt_alarm_start(&timer_camera,camera_timeout,camera_timer_isr,timerContext);
					}
				}
				else if ((current_state4 == rr1_4) || (current_state4 == rr2_4)){
					fprintf(uart,"Camera activated\r\n");
					fprintf(uart,"Snapshot taken\r\n");
				}
			}
			else{
				alt_alarm_stop(&timer_camera);
				alt_alarm_stop(&timer_vehicle);
				fprintf(uart,"Vehicle left\r\n");
				fprintf(uart,"Vehicle was in the intersection for %d ms.\r\n",count);
				count = 0;
			}
		}
	}

	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0);
}

//LCD writing function, pass in the current mode
int lcd_set_mode(int mode){
	if (mode != mode_display) {
		if(lcd != NULL){
			#define ESC 27
			#define CLEAR_LCD_STRING "[2J"
			fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
			fprintf(lcd, "CURRENT MODE: %d\n", mode);
			mode_display = mode; //set mode display state to prevent LCD flickering
		}
	}
	return 0;
}

// Mode 1
// Simple controller with automatic lights
int simple_tlc(){
	void* timerContext = 0;
	//loop through all states, starting timer on current state and setting outputs
	if (current_state1 == rr1_1){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b100100); //turn RED-RED on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, rrrr, simple_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state1 == gr_1){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b100001); //turn GREEN-RED on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, grrg, simple_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state1 == yr_1){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b100010); //turn YELLOW-RED on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, yrry, simple_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state1 == rr2_1){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b100100); //turn RED-RED on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, rrrr, simple_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state1 == rg_1){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b001100); //turn RED-GREEN on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, grrg, simple_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state1 == ry_1){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b010100); //turn RED-YELLOW on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, yrry, simple_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	return 0;
}

// Mode 2
// Allows Pedestrian Inputs
int pedestrian_tlc() {
	void* timerContext = 0;
	//loop through all states, starting timer on current state and setting outputs
	if (current_state2 == rr1_2){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00100100); //turn RED-RED on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, rrrr, pedestrian_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state2 == gr_2){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00100001); //turn GREEN-RED on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, grrg, pedestrian_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state2 == grp1_2){
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b01100001); //turn GREEN-RED and PEDNS on
			if (!(timer_has_started)){
				alt_alarm_start(&timer_simple, grrg, pedestrian_tlc_timer_isr, timerContext);
				timer_has_started = 1;
			}
	}
	else if (current_state2 == yr_2){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00100010); //turn YELLOW-RED on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, yrry, pedestrian_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state2 == rr2_2){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00100100); //turn RED-RED on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, rrrr, pedestrian_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state2 == rg_2){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00001100); //turn RED-GREEN on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, grrg, pedestrian_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state2 == rgp2_2){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b10001100); //turn RED-GREEN and PEDEW on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, grrg, pedestrian_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state2 == ry_2){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00010100); //turn RED-YELLOW on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, yrry, pedestrian_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	return 0;
}

// Mode 3
// Allows the timer to be change in PuTTY
int configurable_tlc(){
	void* timerContext = 0;
	unsigned int switch_value = IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE);

	//loop through all states, starting timer on current state and setting outputs
	if (current_state3 == rr1_3){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00100100); //turn RED-RED on
		//check switch value to see if there are new timer values
		switch_value = IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE);

		if (switch_value & (1<<17)){
			//if switch 17 indicates configuration mode, block until complete valid string is received
			unsigned int strcomplete = 0;
			unsigned int i = 0; //index for string retrieved from UART
			char **splitstrings; //pointer to array of c-strings
			int numbers[10]; //array to store timeout values received from UART

			//allocate enough memory for array of c-strings
			splitstrings = (char**)malloc(10);
			//allocate enough memory for each c-string in array
			for (int j = 0; j < sizeof(splitstrings); ++j) {
				splitstrings[j] = (char*)malloc(10);
			}
			//block code and get input from UART stream
			while (strcomplete == 0){
				config_values[i] = fgetc(uart);
				printf("%c",config_values[i]);
				++i;
				if (config_values[i-1] == '\n'){

					//add NULL to end of string to indicate end of string
					config_values[i] = '\0';
					//move iterator to position 0 (effectively flushing the buffer)
					i = 0;
					unsigned int k = 1; //iterator for array of c-strings

					//use strtok to split the UART string into separate strings from between the commas
					splitstrings[0] = strtok(config_values,comma);
					while (splitstrings[k-1] != NULL) {
							splitstrings[k] = strtok(NULL, comma);
							++k;
					}

					k = 0; //reset iterator for array of c-strings
					//do base-10 conversion of split strings to integers using strtol
					//this returns 0 for an invalid integer, so we are assuming a 0ms timeout is invalid
					while (splitstrings[k] != NULL){
						numbers[k] = strtol(splitstrings[k],NULL,10);
						++k;
					}

					if (k == 6){
						unsigned int notinrange = 0;
						for (int j = 0; j < 6; ++j){
							if ((numbers[j] <= 0) || (numbers[j] > 9999)){
								notinrange = 1;
								printf("Numbers out of range, please re-enter numbers \n");
								break;
							}
						}
						if (!(notinrange)){
							strcomplete = 1;
						}
					}
				}
			}
			//since numbers have been discovered to be valid, assign them to the timer controlling variables
			t1 = numbers[0];
			t2 = numbers[1];
			t3 = numbers[2];
			t4 = numbers[3];
			t5 = numbers[4];
			t6 = numbers[5];
		}
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, t1, configurable_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state3 == gr_3){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00100001); //turn GREEN-RED on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, t2, configurable_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state3 == grp1_3){
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b01100001); //turn GREEN-RED and PEDNS on
			if (!(timer_has_started)){
				alt_alarm_start(&timer_simple, t2, configurable_tlc_timer_isr, timerContext);
				timer_has_started = 1;
			}
	}
	else if (current_state3 == yr_3){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00100010); //turn YELLOW-RED on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, t3, configurable_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state3 == rr2_3){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00100100); //turn RED-RED on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, t4, configurable_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state3 == rg_3){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00001100); //turn RED-GREEN on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, t5, configurable_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state3 == rgp2_3){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b10001100); //turn RED-GREEN and PEDEW on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, t5, configurable_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state3 == ry_3){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00010100); //turn RED-YELLOW on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, t6, configurable_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	return 0;
}

// Mode 4
// Includes a Camera
int camera_tlc(){
	void* timerContext = 0;
	unsigned int switch_value = IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE);

	//loop through all states, starting timer on current state and setting outputs
	if (current_state4 == rr1_4){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00100100); //turn RED-RED on
		//check switch value to see if there are new timer values
		switch_value = IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE);

		if (switch_value & (1<<17)){
			//if switch 17 indicates configuration mode, block until complete valid string is received
			unsigned int strcomplete = 0;
			unsigned int i = 0; //index for string retrieved from UART
			char **splitstrings; //pointer to array of c-strings
			int numbers[10]; //array to store timeout values received from UART

			//allocate enough memory for array of c-strings
			splitstrings = (char**)malloc(10);
			//allocate enough memory for each c-string in array
			for (int j = 0; j < sizeof(splitstrings); ++j) {
				splitstrings[j] = (char*)malloc(10);
			}
			//block code and get input from UART stream
			while (strcomplete == 0){
				config_values[i] = fgetc(uart);
				printf("%c",config_values[i]);
				++i;
				if (config_values[i-1] == '\n'){

					//add NULL to end of string to indicate end of string
					config_values[i] = '\0';
					//move iterator to position 0 (effectively flushing the buffer)
					i = 0;
					unsigned int k = 1; //iterator for array of c-strings

					//use strtok to split the UART string into separate strings from between the commas
					splitstrings[0] = strtok(config_values,comma);
					while (splitstrings[k-1] != NULL) {
							splitstrings[k] = strtok(NULL, comma);
							++k;
					}

					k = 0; //reset iterator for array of c-strings
					//do base-10 conversion of split strings to integers using strtol
					//this returns 0 for an invalid integer, so we are assuming a 0ms timeout is invalid
					while (splitstrings[k] != NULL){
						numbers[k] = strtol(splitstrings[k],NULL,10);
						++k;
					}

					if (k == 6){
						unsigned int notinrange = 0;
						for (int j = 0; j < 6; ++j){
							if ((numbers[j] <= 0) || (numbers[j] > 9999)){
								notinrange = 1;
								fprintf(uart,"Numbers out of range, please re-enter numbers\r\n");
								break;
							}
						}
						if (!(notinrange)){
							strcomplete = 1;
						}
					}
				}
			}
			//since numbers have been discovered to be valid, assign them to the timer controlling variables
			t1 = numbers[0];
			t2 = numbers[1];
			t3 = numbers[2];
			t4 = numbers[3];
			t5 = numbers[4];
			t6 = numbers[5];
		}

		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, t1, camera_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state4 == gr_4){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00100001); //turn GREEN-RED on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, t2, camera_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state4 == grp1_4){
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b01100001); //turn GREEN-RED and PEDNS on
			if (!(timer_has_started)){
				alt_alarm_start(&timer_simple, t2, camera_tlc_timer_isr, timerContext);
				timer_has_started = 1;
			}
	}
	else if (current_state4 == yr_4){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00100010); //turn YELLOW-RED on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, t3, camera_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state4 == rr2_4){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00100100); //turn RED-RED on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, t4, camera_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state4 == rg_4){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00001100); //turn RED-GREEN on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, t5, camera_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state4 == rgp2_4){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b10001100); //turn RED-GREEN and PEDEW on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, t5, camera_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	else if (current_state4 == ry_4){
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b00010100); //turn RED-YELLOW on
		if (!(timer_has_started)){
			alt_alarm_start(&timer_simple, t6, camera_tlc_timer_isr, timerContext);
			timer_has_started = 1;
		}
	}
	return 0;
}

int main() {
	unsigned int switch_value = 0;
	void* context = 0;
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0);
	//enable interrupts for all buttons
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(BUTTONS_BASE, 0x7);
	//register the button interrupt ISR
	alt_irq_register(BUTTONS_IRQ, context, button_interrupt);
	//turn on the LCD
	lcd = fopen(LCD_NAME, "w");
	uart = fopen(UART_NAME, "r+"); //Open an existing file for both reading and writing.
	//The initial contents of the file are unchanged and the initial file position is at the beginning of the file.

	while(1) {
		//read switch value and bitmask to check specific switches (descending priority)
		switch_value = IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE);
		//set mode request depending on switch configuration
		//priority encoded (descending order, i.e. mode 4 is highest and mode 1 is lowest)
		if ((1<<3 & switch_value)) {
			mode_request = 4;
		}
		else if ((1<<2 & switch_value)) {
			mode_request = 3;
		}
		else if ((1<<1 & switch_value)) {
			mode_request = 2;
		}
		else if ((1<<0 & switch_value)) {
			mode_request = 1;
		}
		else
			mode_request = 0;

		//if there is a new mode request:
		//check if we are at a safe state (corresponding to the mode we are in)
		//if safe, then change mode (and make sure to reset all states and interrupt flags)
		if (current_mode != mode_request) {
			switch(current_mode){
			case 1:
				if ((current_state1 == rr1_1) || (current_state1 == rr2_1)){
					current_mode = mode_request;
					reset_volatiles();
				}
				break;
			case 2:
				if ((current_state2 == rr1_2) || (current_state2 == rr2_2)){
					current_mode = mode_request;
					reset_volatiles();
				}
				break;
			case 3:
				if ((current_state3 == rr1_3) || (current_state3 == rr2_3)){
					current_mode = mode_request;
					reset_volatiles();
				}
				break;
			case 4:
				if ((current_state4 == rr1_4) || (current_state4 == rr2_4)){
					current_mode = mode_request;
					reset_volatiles();
				}
				break;
			default: //starting state
				current_mode = mode_request;
				reset_volatiles();
			}
		}


		switch(current_mode){
		case 1:
			simple_tlc();
			lcd_set_mode(1);
			break;
		case 2:
			pedestrian_tlc();
			lcd_set_mode(2);
			break;
		case 3:
			configurable_tlc();
			lcd_set_mode(3);
			break;
		case 4:
			camera_tlc();
			lcd_set_mode(4);
			break;
		default:
			lcd_set_mode(0);
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0b111111); //all LEDs on
		}
	}
	return 0;
}
