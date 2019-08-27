# COMPSYS 303 Assignment 1 Group 23: Traffic Light Controller
## Sherjeel Shehzad and Kevin Tang

### The code contains 4 modes: *Simple, Pedestrian, Configurable and Camera*

#### The following functions have been used for all of the states:

* *void reset_volatiles()*: Resets all volatile variables
* *int lcd_set_mode(int mode)*: Shows the mode on the LCD
* *int main()*: Runs all the functions of each mode and changes modes

In order to change states, SWITCHES 0-3, with SWITCH 3 representing Mode 4 and SWITCH 0 representing Mode 0.
Mode 4 will have the highest priority if multiple switches are flipped whereas Mode 1 will have the least priority.
Once a Mode Request has been detected, the mode will function as normally until the Red-Red(1) state where it will then switch modes.
*WE ASSUME THAT THE MODES BUILD ON TOP OF EACH OTHER.*

### Mode 1: Simple State

The traffic light goes through 6 states in order: **(Red-Red(1), Green-Red, Yellow-Red, Red-Red(2), Red-Green, Red-Yellow)**.
After the Red-Yellow state finishes, it will then loop back to Red-Red(1). 

The state will stay for a certain amount of time (listed below) before switching in ms.

* Red-Red(1) / Red-Red(2): 500
* Green-Red / Red-Green: 6000
* Yellow-Red / Red-Yellow: 2000

#### The mode contains the following functions:

* alt_u32 simple_tlc_timer_isr(void\* context)*: Handles the Simple Mode ISR
* *int simple_tlc()*: Simple Mode logic

#### Console States

| Console State Number | State      |
| -------------------- | ---------: |
| 0                    | Red-Red(1) |
| 1                    | Green-Red  |
| 2                    | Yellow-Red |
| 3                    | Red-Red(2) |
| 4                    | Red-Green  |
| 5                    | Red-Yellow |

### Mode 2: Pedestrian State

The Pedestrian State follows the same pattern as the Simple State. However, it accepts user input to allow pedestrians to cross.
BUTTON 0 correlates to the NS crossing and BUTTON 1 correlates to the EW crossing. 
Once pressed, it queues the controller and will light up once the conditions have been met.
The states will be the same as Mode 1 but will also contain Green-Red-Ped 1 and Red-Green-Ped 2 for the NS and EW crossing respectively.

#### The mode contains the following functions:

* *alt_u32 pedestrian_tlc_timer_isr(void\* context)*: Handles the Pedestrian Mode ISR 
* *void button_interrupt(void\* context, alt_u32 id)*: Handles the button interrupts for Pedestrian Mode input
* *int pedestrian_tlc()*: Pedestrian Mode logic

#### Assumptions

* Green-Red cannot jump to Green-Red-Ped 1 and Red-Green cannot jump to Red-Green-Ped 2 as there is no direct path on the state diagram
* The user will be able to press the button even as the LED is lit, it will light up during the next rotation.

#### Console States

| Console State Number | State            |
| -------------------- | ---------------: |
| 0                    | Red-Red(1)       |
| 1                    | Green-Red        | 
| 2                    | Green-Red-Ped 1  |
| 3                    | Yellow-Red       |
| 4                    | Red-Red(2)       |
| 5                    | Red-Green        |
| 6                    | Green-Red-Ped 2  |
| 7                    | Red-Yellow       |

### Mode 3: Configurable State

The Configurable State allows the user to configure the time between states using UART and PuTTY based on Mode 2.
In order to change the times, the user must flip SWITCH 17 down, wait for Red-Red(1), follow the packet format and end the line with `Alt+L`.
If there are any issues with formatting, the console will output an error message.
The states will be exactly the same as Mode 2.

#### The mode contains the following functions:

* *alt_u32 configurable_tlc_timer_isr(void\* context)*: Handles the Configurable Mode ISR
* *void button_interrupt(void\* context, alt_u32 id)*: Handles the button interrupts for Configurable Mode input
* *int configurable_tlc()*: Configurable Mode logic

#### Assumptions

* Green-Red cannot jump to Green-Red-Ped 1 and Red-Green cannot jump to Red-Green-Ped 2 as there is no direct path on the state diagram
* The user will be able to press the button even as the LED is lit, it will light up during the next set of green light
* The user can either end the line with `\r\n` or just `\n`

#### Console States

| Console State Number | State            |
| -------------------- | ---------------: |
| 0                    | Red-Red(1)       |
| 1                    | Green-Red        | 
| 2                    | Green-Red-Ped 1  |
| 3                    | Yellow-Red       |
| 4                    | Red-Red(2)       |
| 5                    | Red-Green        |
| 6                    | Green-Red-Ped 2  |
| 7                    | Red-Yellow       |

### Mode 4: Camera State

The Camera State allows the user to track the movement of a vehicle travelling through an intersection.
The camera will be activated whenever a vehicle crosses the intersection when the light is yellow.
If the vehicle remains within the intersection after 2 seconds, the camera will take a snapshot of that vehicle. 
Alternatively, if the car enters on a red light then a snapshot should be taken immediately.
The console will then output the time the vehicle is in the intersection.
BUTTON 2 will toggle the camera on and off.
The states will be exactly the same as Mode 3.

#### The mode contains the following functions:

* *alt_u32 camera_tlc_timer_isr(void\* context)*: Handles the overall Camera State switching ISR
* *alt_u32 camera_timer_isr(void\* context)*: Handles the 'Snapshot Taken' and resets the camera timer
* *alt_u32 vehicle_timer_isr(void\* context)*: Handles the timer count
* *void button_interrupt(void\* context, alt_u32 id)*: Handles the button interrupts for Configurable Mode input
* *int camera_tlc()*: Camera Mode logic

#### Assumptions

* Only one vehicle can enter the intersection at a time
* The vehicle will travel through the intersection on the red light regardless of direction

#### Console States

| Console State Number | State            |
| -------------------- | ---------------: |
| 0                    | Red-Red(1)       |
| 1                    | Green-Red        | 
| 2                    | Green-Red-Ped 1  |
| 3                    | Yellow-Red       |
| 4                    | Red-Red(2)       |
| 5                    | Red-Green        |
| 6                    | Green-Red-Ped 2  |
| 7                    | Red-Yellow       |

//TODO: add alt_alarm_stop()