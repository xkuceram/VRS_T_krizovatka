# Traffic Signal Model of a T-Junction

Team project for the VRS course

## Project Overview

We will build a physical model of a T-junction with one pedestrian crossing. An STM32 NUCLEO-F303K8 board will control three vehicle signals and one pedestrian signal. In **Demo** mode, the signals change automatically in a fixed sequence. In **Control** mode, the user can change individual signal colors from a computer by sending commands over USART. In either mode, the user can check the model's status and switch modes from the terminal.

| ID | Location on the model | Signal colors |
| --- | --- | --- |
| A | Approach from the left along the main road | Red, yellow, green |
| B | Approach from the right along the main road | Red, yellow, green |
| C | Approach from the lower leg of the junction | Red, yellow, green |
| P | Pedestrian crossing across the lower leg | Red, green |

Each vehicle signal controls all traffic movements from its approach. The model treats every pair of vehicle approaches as conflicting. The pedestrian crossing has a dedicated phase during which all vehicle signals are red.

## Signal Rules

- Initialization turns on all four red lights. Demo mode starts its first green phase after a one-second all-red interval.
- In each steady state, exactly one light is on at each signal. When a vehicle signal is green or yellow, every other signal is red. When the pedestrian signal is green, all three vehicle signals are red.
- A vehicle green phase ends with two seconds of yellow. The end of a vehicle yellow or pedestrian green is followed by at least one second of all-red before another green can start.
- One state machine enforces these rules in Demo mode, Control mode, and during mode changes. If a command is rejected, the current phase continues on its existing schedule.

## Demo Mode

After the initial one-second all-red interval, the model runs the following cycle with a nominal duration of 30 s. The cycle returns to step 1 after the last phase.

| Step | A | B | C | P | Duration |
| ---: | --- | --- | --- | --- | ---: |
| 1 | Green | Red | Red | Red | 5 s |
| 2 | Yellow | Red | Red | Red | 2 s |
| 3 | Red | Red | Red | Red | 1 s |
| 4 | Red | Green | Red | Red | 5 s |
| 5 | Red | Yellow | Red | Red | 2 s |
| 6 | Red | Red | Red | Red | 1 s |
| 7 | Red | Red | Green | Red | 5 s |
| 8 | Red | Red | Yellow | Red | 2 s |
| 9 | Red | Red | Red | Red | 1 s |
| 10 | Red | Red | Red | Green | 5 s |
| 11 | Red | Red | Red | Red | 1 s |

The state machine uses the millisecond system clock to measure how long the current phase has been active. The main loop continues processing serial communication between signal changes. Yellow and all-red intervals run for their full duration even if the preceding phase ends late.

## Control Mode and Mode Changes

In manual mode, the state machine waits for a command once the all-red interval has elapsed. The user can request green on any of the four signals. The signal stays green until the state machine accepts a command to end the phase or change modes. The vehicle yellow and all-red intervals then run automatically.

When a mode change is requested, a vehicle green switches to yellow, an ongoing yellow runs to completion, and a pedestrian green switches to red. The state machine completes the all-red interval before activating the target mode. If the interval has already elapsed, it switches modes immediately. On entering Demo mode, the cycle starts with green for A. On entering Control mode, all signals are red and the state machine waits for a command.

## Communication Protocol

The computer connects through the ST-LINK virtual serial port. We will use **USART2 at 9600 baud with 8 data bits, no parity, 1 stop bit, and no flow control**. PA2 is reserved for TX and PA15 for RX. [STMicroelectronics UM1956, section 7.9](https://www.st.com/resource/en/user_manual/dm00231744-stm32-nucleo-32-boards-mb1180-stmicroelectronics.pdf)

Commands use uppercase ASCII. A single space separates adjacent tokens. The `<signal>` parameter accepts `A`, `B`, `C`, or `P`. A command line contains at most 32 characters before the `LF` or `CRLF` terminator. The device skips empty lines and sends one response line ending in `CRLF` for each non-empty line. The terminal sends the next command after receiving the response.

| Command | Behavior |
| --- | --- |
| `MODE DEMO` | Requests a switch to Demo mode. If Demo is already active, the device acknowledges the current mode. |
| `MODE CONTROL` | Requests a switch to Control mode. If Control is already active, the device acknowledges the current mode. |
| `SET <signal> GREEN` | Turns the selected signal green once the all-red interval has elapsed. Repeating the command for a signal that is already green returns an acknowledgment. A request to start a new green during an active green or yellow phase or during the all-red interval returns an error. |
| `SET <signal> YELLOW` | Ends green on the selected vehicle signal. The two-second yellow phase is followed by all-red. Applies to A, B, or C while the selected signal is green. |
| `SET <signal> RED` | Ends green on the selected signal. For A, B, and C, the signal first turns yellow. For P, it turns red and starts the all-red interval. If the selected signal is already yellow, the yellow phase runs to completion. If the signal is already red, the device acknowledges its current state. |
| `STATUS` | Reports the active mode, the target mode if a switch is in progress, whether the model is ready to accept a manual green request, and the current colors of A, B, C, and P. |

The device processes `SET` commands in Control mode once any mode change is complete. During a mode change, the user can send `STATUS` or repeat the request for the same target mode. A repeated request returns `OK`, and the transition continues on its original schedule.

| Response | Meaning |
| --- | --- |
| `OK` | The device accepted a valid command. Use `STATUS` to check the progress of a timed transition. |
| `OK MODE=CONTROL NEXT=- READY=1 A=RED B=RED C=RED P=RED` | Example `STATUS` response after entering Control mode. `NEXT` shows the target mode during a mode change and `-` at other times. `READY=1` means the state machine is waiting for a command in Control mode with all signals red and the clearance interval complete. In all other cases, the value is `0`. |
| `ERR SYNTAX` | Unknown command, invalid line format, or unknown mode, signal ID, or color. |
| `ERR LENGTH` | The line exceeded 32 characters. The receiver discards the remainder through the end of the line and then accepts the next command. |
| `ERR BUSY` | During a mode change, the device received a `SET` command or a request to return to the mode it is leaving. |
| `ERR MODE` | The user sent a `SET` command while Demo mode was active. |
| `ERR STATE` | The user requested yellow for P or for a vehicle signal in its red or yellow phase. |
| `ERR CONFLICT` | The user requested a new green during an active green or yellow phase or during the all-red interval. |

## Required Hardware

| Item | Quantity | Purpose |
| --- | ---: | --- |
| STM32 NUCLEO-F303K8 | 1 | Model control, programming, and serial communication through ST-LINK |
| Red LED | 4 | A, B, C, and P |
| Yellow LED | 3 | A, B, and C |
| Green LED | 4 | A, B, C, and P |
| 330 Ω resistor | 11 | One series resistor per LED |
| Breadboard and jumper wires | 1 set | Connect the board to the lights |
| Base and model-building materials | 1 set | Roadway, pedestrian crossing, and signal mounts |
| Computer and USB data cable | 1 set | USB power, firmware upload, and terminal control |

### Wiring Design

We will configure each LED pin as a push-pull output. Each pin connects to its LED anode through a dedicated resistor. The cathodes connect to the board's ground, for example at CN3/4. A logic high turns the LED on. With a 3.3 V output and a 2.0 V LED forward voltage, a 330 Ω resistor gives a current of approximately 3.9 mA. We will check LED visibility and polarity during initial testing. [STMicroelectronics DS9866, GPIO electrical characteristics](https://www.st.com/resource/en/datasheet/stm32f303k8.pdf)

| Signal | Red | Yellow | Green |
| --- | --- | --- | --- |
| A | PA0 · A0 · CN4/12 | PA1 · A1 · CN4/11 | PA3 · A2 · CN4/10 |
| B | PA12 · D2 · CN3/5 | PB0 · D3 · CN3/6 | PB1 · D6 · CN3/9 |
| C | PA8 · D9 · CN3/12 | PA11 · D10 · CN3/13 | PB5 · D11 · CN3/14 |
| P | PA4 · A3 · CN4/9 | — | PA7 · A6 · CN4/6 |

Each entry lists the STM32 pin, the label printed on the board, and the connector position. [STMicroelectronics UM1956, table 12](https://www.st.com/resource/en/user_manual/dm00231744-stm32-nucleo-32-boards-mb1180-stmicroelectronics.pdf)

## Software Module Interfaces

- **State machine.** Receives the current time and requests from the USART module. Determines the mode, phase, and outcome of each request. Its output is one phase from the shared `TrafficPhase` type (all-red, green or yellow for one vehicle signal, or pedestrian green).
- **GPIO driver.** Maps each phase to a complete set of 11 output levels. During a transition, it first turns off the outgoing green or yellow, sets the red lights, and then turns on the next yellow or green according to the new phase. It sets all-red after initialization.
- **USART module.** Receives bytes via interrupts and processes complete lines in the main loop. It turns commands into requests for the state machine and uses the results to build responses. For `STATUS`, it reports the colors of the last phase applied to the GPIO outputs. Reception and transmission both run without blocking the main loop.

## Division of Work

Each team member will design, implement, and verify their own module. They will submit its source files and the results of their three assigned tests. During the project defense, each member will demonstrate their part on the shared model.

| Team member and branch | Design and implementation | Individual deliverables and demonstration |
| --- | --- | --- |
| **Branislav Krajčovič** · `feature/traffic-fsm` | Design the state and transition table. Implement non-blocking timing, the Demo cycle, Control command handling, and mode transitions in `traffic_fsm.c/.h`. | Transition table, state machine source code, and tests F1–F3. Demonstrate the full cycle, clearance intervals, and rejection of a conflicting request. |
| **Miroslav Kučera** · `feature/model-gpio` | Design and build the physical model and wire the 11 LEDs. Configure GPIO in CubeMX and implement phase-to-pin mapping, initialization, and output switching order in `traffic_gpio.c/.h`. | Physical model, wiring diagram, driver source code, and tests H1–H3. Demonstrate the mapping of all LEDs and how they change during phase transitions. |
| **Samuel Jantošovič** · `feature/usart-control` | Design the protocol format and USART2 configuration. Implement interrupt-driven reception and transmission, line assembly, command validation, and conversion of state machine results into responses in `traffic_serial.c/.h`. | Protocol description, communication module source code, and tests U1–U3 with a terminal log. Demonstrate terminal control, status reporting, and handling of invalid input. |

## Functional Testing

| Test | Procedure and expected result |
| --- | --- |
| **F1 – Demo timing** | After initialization, the model displays all-red for 1 s and then runs the sequence A → B → C → P twice. In a test with controlled timing, compare the phases against the Demo table, including when yellow and all-red end. |
| **F2 – Conflicting requests** | In Control mode, request green for B and P while A is green. Then test a request for green on C while P is green. The device returns `ERR CONFLICT` in each case. It also rejects new green requests during yellow and the all-red interval. Once both intervals have elapsed, it accepts a valid request. |
| **F3 – Mode changes** | Request a mode change during a vehicle green and during a pedestrian green. The vehicle signal runs through the full yellow and all-red intervals. A pedestrian green is followed by the all-red interval. Control mode waits for a command, and Demo mode starts with phase A. |
| **H1 – LED mapping** | Use a standalone driver test program to set all eight output phases. For each phase, use the table to verify that the correct LEDs light up on all four signals. |
| **H2 – Initialization** | Reset the board during an active phase. All-red is the first state displayed after GPIO initialization. |
| **H3 – Output switching** | On the model, verify the vehicle transition green → yellow → all-red, followed by green for P. Step through the driver and verify that the write that turns off the outgoing light occurs before the write that turns on the next light. |
| **U1 – Valid commands** | Send `STATUS` with both `LF` and `CRLF`. Send `MODE CONTROL`, wait for `READY=1`, and then send `SET A GREEN`. Verify the `OK` response and check that the status report shows `A=GREEN`. |
| **U2 – Invalid input and receiver recovery** | In Control mode, send `SET X GREEN`, `SET P YELLOW`, and a 33-character line. The device returns `ERR SYNTAX`, `ERR STATE`, and `ERR LENGTH` in that order, then processes a valid `STATUS` command. |
| **U3 – Communication during the cycle** | In Demo mode, send `STATUS` repeatedly and send `SET A RED` once. Status responses show the phases changing, `SET` returns `ERR MODE`, and the cycle follows the Demo table. During a vehicle green, send `MODE CONTROL`. As soon as `OK` arrives, send `SET P GREEN`. The expected response during the mode change is `ERR BUSY`. |

## Repository Workflow

1. On `main`, we will set up the shared STM32CubeIDE project, the pin map, the interface types, and `README.md`. Miroslav will apply the `.ioc` configuration after coordinating with Branislav and Samuel.
2. Each member will develop and test their module on their own branch. A teammate will review each pull request before the changes are merged into `main`.
3. The first integration test will connect the terminal, state machine, and GPIO driver. In Control mode, we will set A to green and verify that a request to turn P green returns `ERR CONFLICT`.
4. Before the project defense, we will build the firmware from `main`, flash it through ST-LINK, and run the tests on the finished model. We will record the results along with the wiring map and an example terminal session.
