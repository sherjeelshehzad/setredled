# COMPSYS 303 Assignment 1 Group 26: Traffic Light Controller
## Sherjeel Shehzad and Kevin Tang

### The code contains 4 modes: *Simple, Pedestrian, Configurable and Camera*

#### The following functions have been used for all of the states:

* *void reset_volatiles()*: Resets all volatile variables
* *int lcd_set_mode(int mode)*: Shows the mode on the LCD
* *int main()*: Runs all the functions of each mode and changes modes

In order to change states, SWITCHES 0-3 will be used, with SWITCH 3 representing Mode 4 and SWITCH 0 representing Mode 0.
Mode 4 will have the highest priority if multiple switches are flipped, whereas Mode 1 will have the least priority.
Once a Mode Request has been detected, the current mode will continue normally until either the Red-Red(1) or Red-Red(2) states where it will then switch modes and reset to the Red-Red(1) state.
Each subsequent mode will build on top of previous modes.

### Mode 1: Simple Mode

The traffic light goes through 6 states in order: **(Red-Red(1), Green-Red, Yellow-Red, Red-Red(2), Red-Green, Red-Yellow)**.
After the Red-Yellow state finishes, it will then loop back to Red-Red(1). 

The states will stay for a certain amount of time (listed below) before switching:

* Red-Red(1) / Red-Red(2): 500 ms
* Green-Red / Red-Green: 6000 ms
* Yellow-Red / Red-Yellow: 2000 ms

#### The mode uses the following functions:

* alt_u32 simple_tlc_timer_isr(void\* context)*: Handles the Simple Timer Interrupts (Mode 1 State Transition Logic)
* *int simple_tlc()*: Handles the Simple Mode output logic

#### Console States

| Console State Number | State      |
| :------------------: | :--------: |
|           0          | Red-Red(1) |
|           1          | Green-Red  |
|           2          | Yellow-Red |
|           3          | Red-Red(2) |
|           4          | Red-Green  |
|           5          | Red-Yellow |

### Mode 2: Pedestrian Mode

The Pedestrian Mode follows the same pattern as the Simple Mode. However, it accepts user input to allow pedestrians to cross.
KEY 0 correlates to the NS crossing and KEY 1 correlates to the EW crossing. 
Once pressed, it queues the pedestrian crossing and will light up the corresponding pedestrian lights once the conditions have been met.
The states will be the same as the Simple Mode (Mode 1), but will additionally contain Green-Red-Ped NS and Red-Green-Ped EW for the NS and EW crossings respectively.

The states will stay for a certain amount of time (listed below) before switching:

* Red-Red(1) / Red-Red(2): 500 ms
* Green-Red / Green-Red-Ped NS / Red-Green / Red-Green-Ped EW: 6000 ms
* Yellow-Red / Red-Yellow: 2000 ms

#### The mode uses the following functions:

* *alt_u32 pedestrian_tlc_timer_isr(void\* context)*: Handles the Pedestrian Timer (Mode 2/3/4 State Transition Logic)
* *void button_interrupt(void\* context, alt_u32 id)*: Handles the button interrupts for Pedestrian Mode input
* *int pedestrian_tlc()*: Handles the Pedestrian Mode output logic

#### Assumptions

* Green-Red cannot jump to Green-Red-PedNS and Red-Green cannot jump to Red-Green-PedEW as there is no direct path between these states on the state diagram
* The user will, however, still be able to press the NS/EW pedestrian button even if the NS/EW traffic LED is lit; the pedestrian LED will light up during the next rotation.

#### Console States (displayed in the NIOS console for debugging purposes)

| Console State Number | State            |
| :------------------: | :--------------: |
|           0          | Red-Red(1)       |
|           1          | Green-Red        | 
|           2          | Green-Red-Ped NS |
|           3          | Yellow-Red       |
|           4          | Red-Red(2)       |
|           5          | Red-Green        |
|           6          | Red-Green-Ped EW |
|           7          | Red-Yellow       |

### Mode 3: Configurable Timers Mode

The Configurable Mode follows the same state transition pattern as the Pedestrian Mode (Mode 2), but it allows the user to configure the time between states using UART inputs.
The states will thus be exactly the same as Mode 2.

By default, the states will stay for a certain amount of time (listed below) before switching or unless changed via UART.
In order to change the configure the switching time, the user must switch SWITCH 17 *up* then wait for the state to be Red-Red(1).
Then, once configuration mode has been entered, they must switch SWITCH 17 *down* and after a valid set of inputs, the updated switching times will immediately take effect.

* Red-Red(1) / Red-Red(2): 500 ms
* Green-Red / Green-Red-Ped 1 / Red-Green / Red-Green-Ped 2: 6000 ms
* Yellow-Red / Red-Yellow: 2000 ms

#### UART Timer Transition Packet Format

#,#,#,#,#,#[\r]\n
* Where '#' is a 1-4 digit integer representing the corresponding timeout value in ms (between 1 and 9999)
* ',' separates the timeout values
* \r signals a carriage return and is optional (`Ctrl+M` or the Enter Key on PuTTY)
* \n signals the end of the inputs (`Ctrl+J` on PuTTY)

If there are any issues with the input format, an error message will be output over UART and will ask the user to enter another set of inputs.

#### The mode uses the following functions:

* *alt_u32 pedestrian_tlc_timer_isr(void\* context)*: Handles the Configurable Mode Timer (Mode 2/3/4 State Transition Logic)
* *void button_interrupt(void\* context, alt_u32 id)*: Handles the button interrupts for Configurable Mode input
* *int configurable_tlc()*: Handles the Configurable Mode output and timer configuration logic

#### Assumptions
* The assumptions from Mode 2 still apply
* The user can either end the input line with `\r\n` or just `\n`
* Only integers will be input for the timer values

#### Console States

| Console State Number | State            |
| :------------------: | :--------------: |
|           0          | Red-Red(1)       |
|           1          | Green-Red        | 
|           2          | Green-Red-Ped NS |
|           3          | Yellow-Red       |
|           4          | Red-Red(2)       |
|           5          | Red-Green        |
|           6          | Red-Green-Ped EW |
|           7          | Red-Yellow       |

### Mode 4: Camera Mode

The Camera Mode builds on the previous modes (thus allows pedestrian inputs and configurable timers), but additionally allows the user to track the movement of a vehicle traveling through an intersection.
The traffic camera will be activated whenever a vehicle crosses the intersection when the light is yellow.
If the vehicle remains within the intersection after 2 seconds, the camera will take a snapshot of that vehicle. 
Alternatively, if the car enters on a red light then a snapshot will be taken immediately.
When the car leaves the intersection, the console will then output the time the vehicle is in the intersection via UART.
BUTTON 2 will simulate the car entering and leaving (via odd and even button presses respectively).
The states and state transitions will be exactly the same as Mode 3.

By default, the state will stay for a certain amount of time (listed below) before switching in ms or unless changed via UART:

* Red-Red(1) / Red-Red(2): 500 ms
* Green-Red / Green-Red-Ped 1 / Red-Green / Red-Green-Ped 2: 6000 ms
* Yellow-Red / Red-Yellow: 2000 ms

#### The mode contains the following functions:

* *alt_u32 pedestrian_tlc_timer_isr(void\* context)*: Handles the Camera Mode Timer (Mode 2/3/4 State Transition Logic)
* *alt_u32 camera_timer_isr(void\* context)*: Handles the 'Snapshot Taken' and resets the camera timer
* *alt_u32 vehicle_timer_isr(void\* context)*: Handles the Vehicle Timer count
* *void button_interrupt(void\* context, alt_u32 id)*: Handles the button interrupts for Configurable Mode Vehicle Crossing input
* *int configurable_tlc()*: Handles the Camera Mode output and timer configuration logic

#### Assumptions

* The assumptions from Modes 2 and 3 still apply
* Only one vehicle can enter the intersection at a time
* The vehicle entering will be assumed to travel through the intersection in the correct (green light) direction
* The camera will only be activated when the vehicle enters on a yellow light, not when the vehicle enters on a green light and the light changes to yellow

#### Console States

| Console State Number | State            |
| :------------------: | :--------------: |
|           0          | Red-Red(1)       |
|           1          | Green-Red        | 
|           2          | Green-Red-Ped NS |
|           3          | Yellow-Red       |
|           4          | Red-Red(2)       |
|           5          | Red-Green        |
|           6          | Red-Green-Ped EW |
|           7          | Red-Yellow       |
