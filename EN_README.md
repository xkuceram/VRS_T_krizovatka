# T-junction traffic light model

Team project for the VRS course <sub>· traffic-fsm · model-gpio · usart-control</sub>

[Slovenčina](README.md) · **English**

The tag at the end of a paragraph refers to the branch listed under [team responsibilities](#team-responsibilities) and identifies who maintains the text. For shared sections, the listed owners agree on changes before opening a pull request. <sub>· traffic-fsm · model-gpio · usart-control</sub>

## Objective and demonstration

We will build a T-junction model with four vehicle signals, one pedestrian signal and two green arrows. The lights connect directly to 12 outputs on an STM32 NUCLEO-F303K8 board. Approach C has one lane shared by left- and right-turning vehicles and one IR sensor at the stop line. Pedestrians use a push button to request a green signal. <sub>· traffic-fsm · model-gpio</sub>

During the project demonstration, we will run the Demo cycle with approach C both clear and occupied. We will demonstrate which movements can run together when vehicles turn from C and measure the yellow and all-red intervals. In Control mode, we will operate the signals from a serial terminal and verify that the controller rejects a conflicting command. <sub>· traffic-fsm · model-gpio · usart-control</sub>

## Contents

<sub>traffic-fsm · model-gpio · usart-control</sub>

- [Project scope](#project-scope)
- [System overview](#system-overview)
- [Serial protocol](#serial-protocol)
- [Required hardware](#required-hardware)
  - [Wiring schematic](#wiring-schematic)
  - [LED wiring](#led-wiring)
- [Cost estimate](#cost-estimate)
- [Team responsibilities](#team-responsibilities)
- [Testing](#testing)
- [Learning objectives](#learning-objectives)
- [Repository workflow](#repository-workflow)

## Project scope

The main road runs from left to right in the layout. Approach B on the right has one lane for straight-ahead traffic and one for traffic turning onto the lower branch. The lower branch C has a shared approach lane for both turns and a separate exit lane for vehicles turning from A or B. We monitor whether the approach lane is occupied at the stop line on C. <sub>· traffic-fsm · model-gpio</sub>

<sub>traffic-fsm · model-gpio</sub>

| Signal | Approach | Movement permitted on green | Signal colours |
| --- | --- | --- | --- |
| `A` | From the left | Continue straight towards B or turn onto the lower branch towards C | Red, yellow, green |
| `A_S` | From the left | Turn onto the lower branch towards C while `A` is red | Green arrow |
| `B_R` | From the right | Continue straight along the main road towards A | Red, yellow, green |
| `B_D` | From the right | Turn onto the lower branch towards C | Red, yellow, green |
| `C` | Shared approach from below | Turn left from C towards A | Red, yellow, green |
| `C_S` | Shared approach from below | Turn right from C towards B while `C` is red | Green arrow |
| `P` | Crossing on the lower branch | Cross the road | Red, green |

![T-junction layout with a shared approach lane on C and an IR sensor at the stop line](docs/images/t-krizovatka.svg)

*Figure 1. Turning paths, the shared approach and separate exit lane on C, and the IR sensor at the stop line.* <sub>· model-gpio</sub>

### Signal rules

- At startup, all five signals show red and both arrows are off. The first green can begin after one second of all red. <sub>· traffic-fsm · model-gpio</sub>
- Each standard signal shows one colour at a time in a steady phase. Arrow `A_S` can light only while `A` is red. A right turn from C requires arrow `C_S` and a red `C` signal. When the pedestrian signal is green, all four vehicle signals are red and both arrows are off. <sub>· traffic-fsm</sub>
- The controller permits only the vehicle combinations in the Demo table and the selections in the Control command table. For example, `A_S` and `B_D` lead into the same exit lane and run in separate phases. <sub>· traffic-fsm · usart-control</sub>
- At the end of a phase, the arrows turn off and all standard vehicle signals that were green switch to yellow together for 2 s. A phase with only an arrow or the pedestrian signal goes directly to all red. The controller holds all red for at least 1 s before another green. <sub>· traffic-fsm</sub>

### Pedestrian push button and detection on C

We will place the button beside crossing P. We will debounce the input so that one press registers one request, even when the button is held down. <sub>· model-gpio</sub>

The controller serves the request when P turns green. A press during the pedestrian green is covered by the current phase. A reset clears any pending request. <sub>· traffic-fsm</sub>

A single IR sensor at the stop line on C monitors the shared approach lane. We will check its detection range with a stationary car and with a car passing in the adjacent exit lane. <sub>· model-gpio</sub>

### Demo mode

When `A` is green, a vehicle can turn from A onto C across the pedestrian crossing. Pedestrians therefore get a separate green phase with all vehicle signals red and both arrows off. <sub>· traffic-fsm</sub>

In the table, **1** means green or an illuminated arrow. **0** means red or an arrow that is off. Yellow is used during the transition between phases. <sub>· traffic-fsm</sub>

<sub>traffic-fsm</sub>

| Phase | `A` | `A_S` | `B_R` | `B_D` | `C` | `C_S` | `P` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| A | 1 | 0 | 1 | 0 | 0 | 0 | 0 |
| Left turn from C | 0 | 1 | 0 | 0 | 1 | 0 | 0 |
| B with a right turn from C | 0 | 0 | 1 | 1 | 0 | 1 | 0 |
| B with approach C clear | 0 | 0 | 1 | 1 | 0 | 0 | 0 |
| Pedestrians | 0 | 0 | 0 | 0 | 0 | 0 | 1 |
| All red | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

Demo mode starts with phase A after the all-red interval. After phase A ends and the one-second clearance interval has elapsed, the controller checks whether approach C is occupied. The state machine records this as `CAR_C`. <sub>· traffic-fsm</sub>

- With `CAR_C=0`, the controller runs B with approach C clear and then returns to A. <sub>· traffic-fsm</sub>
- With `CAR_C=1`, the controller runs the left turn from C, then B with a right turn from C, and returns to A. The single sensor detects whether the approach lane is occupied, so this cycle allocates time to both turns. The right-turn phase still runs if the lane clears during the left-turn phase. The controller checks occupancy again after the next phase A. <sub>· traffic-fsm</sub>

During the left-turn phase, a vehicle from C joins the lane towards A. Arrow `A_S` also permits a vehicle from A to turn into the separate exit lane on the lower branch. In the B phase with a right turn from C, `B_R` permits straight-ahead traffic towards A, `B_D` permits traffic into the exit lane on C, and `C_S` permits a right turn onto the main road towards B. We will use the paths in Figure 1 to test these movements with model cars. <sub>· traffic-fsm · model-gpio</sub>

For the model, we propose 5 s of green in a vehicle phase, 2 s of yellow and at least 1 s of all red. If a pedestrian request is pending, the controller inserts phase P after the vehicle phase and the one-second clearance interval. P stays green for 5 s, followed by all red and then the previously scheduled vehicle phase. <sub>· traffic-fsm</sub>

The controller measures elapsed time from the start of each phase using the system time in milliseconds. The main loop continues to read inputs and handle serial communication. The controller runs the yellow and all-red intervals for their full duration even if the preceding phase ends late. <sub>· traffic-fsm</sub>

### Control mode and mode changes

After entering Control mode, all signals are red and both arrows are off. The operator can select one signal, one arrow or both B signals together. The green lasts until a valid command ends the phase or changes the mode. The controller accepts a new green request when `READY=1`. During the all-red clearance interval it returns `ERR CONFLICT`, and the operator can repeat the command after the interval. <sub>· traffic-fsm · usart-control</sub>

The controller continues to read the sensor and push button in Control mode. `STATUS` reports whether approach C is occupied and whether a pedestrian request is pending. Use `SET C GREEN` for a left turn from C and `SET C_S GREEN` for a right turn. `SET P GREEN` turns P green and serves the pedestrian request. <sub>· traffic-fsm · usart-control</sub>

When changing modes, the controller ends the active phase according to the same signal rules. It lets any active vehicle yellow interval finish, turns off the arrows and sets P to red. The requested mode starts after the all-red interval. If the one-second interval has already elapsed, the mode changes immediately. The controller retains any pending pedestrian request across the change. Returning to Demo starts the cycle at phase A. <sub>· traffic-fsm</sub>

## System overview

<sub>traffic-fsm · model-gpio · usart-control</sub>

```mermaid
flowchart TD
    PC["Serial terminal"] <-->|"USART2 through ST-LINK"| USART["USART module"]
    USART <-->|"Commands and status"| FSM["State machine"]
    IN["Push button P and IR sensor C"] --> GPIO
    TICK["System time in ms"] --> FSM
    FSM <-->|"Phase and input states"| GPIO["GPIO driver"]
    GPIO -->|"12 GPIO outputs"| LED["Five RGB LEDs and two arrows"]
```

- **State machine.** It selects phases based on occupancy on C and pedestrian requests, measures intervals and checks commands from the terminal. It passes the requested phase to the GPIO driver. <sub>· traffic-fsm</sub>
- **GPIO driver.** It reads the push button and the IR sensor. It sets the red and green channels of each RGB signal and the state of both arrows for the current phase. <sub>· model-gpio</sub>
- **USART module.** It assembles incoming characters into lines, passes requests to the state machine and sends replies to the terminal. <sub>· usart-control</sub>

## Serial protocol

The computer connects through the ST-LINK virtual COM port. We will set USART2 and the terminal to **9600 baud**, 8 data bits, no parity, 1 stop bit and flow control disabled. The board connects PA2 to ST-LINK for TX and PA15 for RX. <sub>· usart-control</sub> [STMicroelectronics UM1956, Section 7.9](https://www.st.com/resource/en/user_manual/um1956-stm32-nucleo32-boards-mb1180-stmicroelectronics.pdf)

Commands use uppercase ASCII text with a single space between fields. The `<target>` parameter is `A`, `A_S`, `B_R`, `B_D`, `C`, `C_S` or `P`. Target `B` selects both signals on the right-hand approach. An incoming command has at most 32 characters before `LF` or `CRLF`. The device skips empty lines and sends one `CRLF`-terminated reply for each non-empty line. The terminal sends the next command after receiving the reply. <sub>· usart-control</sub>

<sub>traffic-fsm · usart-control</sub>

| Command | Behaviour |
| --- | --- |
| `MODE DEMO` | Requests a change to Demo. In steady Demo mode, the controller acknowledges the current state. |
| `MODE CONTROL` | Requests a change to Control. In steady Control mode, the controller acknowledges the current state. |
| `SET <target> GREEN` | Acknowledges the current state if all requested signals are already green. Starts a new green when `READY=1`. `A_S` and `C_S` light the arrow while the corresponding standard signal stays red. `B` turns both signals on the right green. During a different active phase, yellow or the all-red clearance interval, the controller returns `ERR CONFLICT`. |
| `SET <target> YELLOW` | Ends the current green if the target includes a standard vehicle signal that is green. All standard signals that were green in that phase turn yellow for 2 s. |
| `SET <target> RED` | Ends the entire active phase if the target includes a signal that is green. Vehicle green goes through yellow. An arrow turns off and P goes directly to red. If the target shows yellow or red, or the target arrow is already off, the controller acknowledges the current state and continues the current timing. |
| `STATUS` | Reports the mode, mode change in progress, readiness, signal colours, arrow states, occupancy of approach C and a pending pedestrian request. |

The controller accepts `SET` commands in Control after any mode change has finished. `STATUS` is available during the change. Repeating a request for the same destination mode returns `OK` while the current transition continues. <sub>· traffic-fsm · usart-control</sub>

<sub>traffic-fsm · usart-control</sub>

| Reply | Meaning |
| --- | --- |
| `OK` | The controller accepted the command. `STATUS` shows the progress of a timed transition. |
| `ERR SYNTAX` | Unknown command, target, mode or colour, or an invalid line format. |
| `ERR LENGTH` | The line exceeded 32 characters. The receiver discards the rest of the line and then accepts the next command. |
| `ERR BUSY` | During a mode change, a `SET` command arrived or the operator requested a return to the mode being left. |
| `ERR MODE` | The operator sent `SET` in Demo mode. |
| `ERR STATE` | The yellow command targets no standard vehicle signal that is currently green. |
| `ERR CONFLICT` | A new green was requested during an active phase, yellow or the all-red clearance interval. |

Example `STATUS` reply in Control with a car at the stop line on C and a pending pedestrian request <sub>· usart-control</sub>

```text
OK MODE=CONTROL NEXT=- READY=1 A=RED A_S=OFF B_R=RED B_D=RED C=RED C_S=OFF P=RED CAR_C=1 P_REQ=1
```

`NEXT` shows the requested mode during a change and `-` at other times. `READY=1` means Control can accept a new green. The arrow states are reported as `ON` and `OFF`. `CAR_C=1` means the approach is occupied and `CAR_C=0` means it is clear. `P_REQ=1` indicates a pending pedestrian request. When P turns green, `P_REQ` becomes `0`. <sub>· traffic-fsm · usart-control</sub>

## Required hardware

<sub>model-gpio</sub>

| Item | Quantity | Use |
| --- | ---: | --- |
| STM32 NUCLEO-F303K8 | 1 | Control, programming and serial communication through ST-LINK |
| Diffused 5 mm common-cathode RGB LED | 5 | `A`, `B_R`, `B_D`, `C` and `P` |
| Green 5 mm LED | 2 | Arrows `A_S` and `C_S` |
| 330 Ω, ¼ W resistor | 12 | One for each red and green RGB channel and each green arrow |
| Normally open (NO) push button | 1 | Pedestrian request at crossing P |
| Reflective IR sensor with digital output and 3.3 V supply | 1 | Vehicle detection at the stop line on approach C |
| 830-point breadboard | 1 | Mounting the board and testing the wiring |
| Male-to-male jumper wires | 1 pack (40 pcs) | Breadboard connections |
| Computer and Micro-B USB data cable | 1 set | USB power, firmware flashing and the serial terminal through ST-LINK |

### Wiring schematic

<sub>model-gpio</sub>

![Wiring schematic in KiCad](docs/schematic/schematic.svg)

The `TSSP58038` label and the push-button symbol in KiCad are illustrative. On approach C, we will use an available reflective IR module with a 3.3 V-compatible digital output. The schematic shows its three connections for the 3.3 V supply, GND and `OUT`. We will check the pin labels on the selected module and verify its output logic level during vehicle detection. <sub>· model-gpio</sub>

At crossing P, we will use an available normally open push button. We will wire its switch contacts so that pressing the button connects the input to ground. <sub>· model-gpio</sub>

### LED wiring

We will connect the common cathode of each RGB LED to GND. The red and green anodes will connect to separate GPIO outputs, each through its own series resistor. The blue pin stays disconnected. For a vehicle signal, lighting the red and green channels together produces yellow. The pedestrian signal uses red and green. <sub>· model-gpio</sub>

<sub>model-gpio</sub>

| Signal colour | Red channel R | Green channel G |
| --- | ---: | ---: |
| Red | 1 | 0 |
| Yellow (vehicle signals only) | 1 | 1 |
| Green | 0 | 1 |

We will use twelve identical 330 Ω series resistors. With one RGB LED connected to the board's 3.3 V outputs, we will first check the brightness of red and green and the resulting shade of yellow when both channels are on. The green channel has a higher forward voltage, so it may be dimmer with the same resistor value. <sub>· model-gpio</sub> [RGB LED specifications](https://techfun.sk/produkt/led-rgb-5-mm/) · [STM32F303x6/x8 datasheet, GPIO characteristics](https://www.st.com/resource/en/datasheet/stm32f303k8.pdf)

<sub>model-gpio</sub>

| Light | Board label | STM32 pin | Series resistor |
| --- | --- | --- | ---: |
| `A` R | A0 | PA0 | 330 Ω |
| `A` G | A1 | PA1 | 330 Ω |
| `B_R` R | A2 | PA3 | 330 Ω |
| `B_R` G | A3 | PA4 | 330 Ω |
| `B_D` R | A6 | PA7 | 330 Ω |
| `B_D` G | D9 | PA8 | 330 Ω |
| `C` R | D3 | PB0 | 330 Ω |
| `C` G | D6 | PB1 | 330 Ω |
| `P` R | D11 | PB5 | 330 Ω |
| `P` G | D12 | PB4 | 330 Ω |
| `A_S` green | D10 | PA11 | 330 Ω |
| `C_S` green | D13 | PB3 | 330 Ω |

With the default board configuration, the on-board user LED LD3 lights along with `C_S` because it is connected to D13. <sub>· model-gpio</sub>

In CubeMX, we will configure all 12 pins as `GPIO_Output`, `Push Pull` and `Low Speed`, with internal pull resistors disabled. The five red outputs (PA0, PA3, PA7, PB0, PB5) will start at `High`. The green outputs and arrows will start at `Low`. The `Serial Wire` debug setting leaves PB3 and PB4 available as GPIO. <sub>· model-gpio</sub>

The NUCLEO-F303K8 documentation specifies the board labels, the LD3 connection to D13 and the USB cable type. <sub>· model-gpio</sub> [STMicroelectronics UM1956, Sections 3.1 and 7.5, Table 12](https://www.st.com/resource/en/user_manual/um1956-stm32-nucleo32-boards-mb1180-stmicroelectronics.pdf)

## Cost estimate

The estimate assumes a single Techfun order. Prices include VAT and reflect offers checked on 16 November 2026. For items with multiple variants, we will check availability for the chosen variant when ordering. The sensor and push button in the table are examples of compatible parts. We count the NUCLEO board, computer and USB data cable as equipment for assembly and testing. <sub>· model-gpio</sub>

<sub>model-gpio</sub>

| Item | Quantity | Total price |
| --- | ---: | ---: |
| [Diffused 5 mm common-cathode RGB LED](https://techfun.sk/produkt/led-rgb-5-mm/) | 5 pcs | €0.75 |
| [5 mm LED, green variant](https://techfun.sk/produkt/led-ka-1-kus-5mm-rozne-farby/) | 2 pcs | €0.06 |
| [330 Ω, ¼ W resistors](https://techfun.sk/produkt/rezistor-rozne-hodnoty-1-4w-10-kusov/) | 1 pack (20 pcs) | €0.25 |
| [12 × 12 × 4.3 mm push button](https://techfun.sk/produkt/tlacidlo-1-kus-12x12x4-3-mm/) | 1 pc | €0.08 |
| [Reflective IR module with digital output and 3.3–5 V supply range](https://techfun.sk/produkt/infracerveny-senzor-pre-vyhybanie-sa-prekazkam/) | 1 pc | €0.65 |
| [830-point breadboard](https://techfun.sk/produkt/nepajive-pole-830-bodov/) | 1 pc | €1.60 |
| [10 cm male-to-male jumper wires](https://techfun.sk/produkt/kabliky-40-kusov-10-cm/) | 1 pack (40 pcs) | €1.45 |
| **Parts subtotal** | | **€4.84** |
| [GLS ParcelShop delivery](https://techfun.sk/sposob-platby-a-dorucenia/) | 1 shipment | €2.75 |
| **Total with delivery** | | **€7.59** |

If the signals sit farther from the breadboard, a pack of 20 cm jumper wires costs €2.20. With these wires, the order totals €8.34 including delivery. <sub>· model-gpio</sub> [20 cm jumper wires](https://techfun.sk/produkt/kabliky-40-kusov-20-cm/)

If we also need a data cable for the board, a 1 m USB-A to Micro-B cable is listed separately at €1.29 plus delivery. <sub>· model-gpio</sub> [Solight SSC1301E](https://www.solight.sk/solight-usb-kabel-usb-2-0-a-konektor--usb-b-micro-konektor-sacek-1m-detail-6021000201.aspx)

## Team responsibilities

Each team member will design, program and test their own module. They will submit its source files and the results of the three tests listed for their area. During the project demonstration, each member will show their work on the shared model. <sub>· traffic-fsm · model-gpio · usart-control</sub>

<sub>traffic-fsm · model-gpio · usart-control</sub>

| Member and branch | Design and implementation | Deliverable and demonstration |
| --- | --- | --- |
| **Branislav Krajčovič** · `feature/traffic-fsm` | Develop the state machine, phase combinations, scheduling of both turns when approach C is occupied, and clearance intervals. Handle pedestrian requests and evaluate Control commands in the state machine. | Transition table, state machine code and tests F1–F3. Demonstrate both sets of simultaneous movements involving turns from C and the rejection of a conflicting request. |
| **Miroslav Kučera** · `feature/model-gpio` | Build the model and connect five RGB LEDs, two arrows, the push button and the IR sensor on C. Configure GPIO in CubeMX. Implement the output driver, initialisation and input handling. | Model, wiring schematic, GPIO driver and tests H1–H3. Demonstrate both arrows, the separate B signals and vehicle detection at the stop line on C. |
| **Samuel Jantošovič** · `feature/usart-control` | Design the protocol format and USART2 configuration. Implement reception and transmission, line assembly, syntax checking and reply formatting. Pass valid requests to the state machine. | Protocol description, serial communication code and tests U1–U3 with terminal logs. Demonstrate manual control and handling of an invalid command. |

## Testing

| Test | Procedure and expected result | Owner |
| --- | --- | --- |
| **F1 – Servicing approach C** | With the approach clear, verify A → B → A. With a car at the stop line, verify A → left turn from C → B with a right turn from C → A. Remove the car during the left-turn phase and check that the scheduled phase with `C_S` still runs. | <sub>traffic-fsm</sub> |
| **F2 – Arrows, conflicts and pedestrians** | Check the pair `C` and `A_S` and the group `B_R`, `B_D` and `C_S`. Press the button while A is green. P turns green after yellow and all red. Check that all vehicle movements are prohibited while P is green. In Control with `B_D` green, send `SET A_S GREEN` and expect `ERR CONFLICT`. | <sub>traffic-fsm</sub> |
| **F3 – Mode change** | During B with a right turn from C, press the button and request Control. Verify that `C_S` turns off, both B signals turn yellow, and all red follows. When `STATUS` shows `MODE=CONTROL` and `READY=1`, check `P_REQ=1` and turn P green. Request Demo while P is green. Phase A should start after the one-second clearance interval. | <sub>traffic-fsm</sub> |
| **H1 – LED mapping** | On the workbench, turn on each of the 12 outputs individually. Verify the channels and arrows. For each vehicle signal, turn on R and G together and check that yellow is distinguishable from red and green. | <sub>model-gpio</sub> |
| **H2 – Initialisation and transition** | Reset the board during an active phase and check that all five signals first light red. When the phase changes, verify yellow and all red before the next green lights. | <sub>model-gpio</sub> |
| **H3 – Reading inputs** | Put a car at the stop line on C. Verify `CAR_C=1` while it is stopped and `CAR_C=0` after it leaves. A car passing in the exit lane should leave `CAR_C=0`. Holding the button should register a single press, with another press registered after release and a new press. | <sub>model-gpio</sub> |
| **U1 – Control and status** | When `READY=1`, try `SET B_R GREEN`, `SET B_D GREEN`, `SET C GREEN` and `SET C_S GREEN`. Before requesting each new green, end the previous phase and wait for `READY=1`. Send `STATUS` with both `LF` and `CRLF` and verify `CAR_C` and `P_REQ`. | <sub>usart-control</sub> |
| **U2 – Invalid command and receiver recovery** | In steady Control mode, send `SET X GREEN`, `SET A_S YELLOW` and a 33-character line. Expect `ERR SYNTAX`, `ERR STATE` and `ERR LENGTH`. Check that the following `STATUS` command is processed normally. | <sub>usart-control</sub> |
| **U3 – Commands during operation** | In Demo, send `SET A RED` and expect `ERR MODE`. During green, send `MODE CONTROL`. Immediately after `OK`, send `SET P GREEN` and expect `ERR BUSY` while the mode is changing. | <sub>usart-control</sub> |

### Integrated test on the assembled model

1. After a reset, check all red and the A → B cycle with approach C clear. Place a car at the stop line on C to trigger both phases for turns from C. Guide the model cars along the marked permitted paths and check the clearance between them. Use video to measure the green, yellow and all-red intervals. <sub>· traffic-fsm · model-gpio</sub>
2. During B with a right turn from C, press the button. P turns green after `C_S` goes off, both B signals turn yellow, and the all-red interval ends. Verify that all vehicle signals are red and both arrows are off. After P ends, verify that the controller continues with phase A. <sub>· traffic-fsm · model-gpio</sub>
3. In Control with `READY=1`, send `SET A_S GREEN`. A should show red with the arrow on. `SET B_D GREEN` should return `ERR CONFLICT`. Send `SET A_S RED`, wait for the all-red clearance interval to end, then repeat the command for `B_D` and check `OK` and the signal on the model. <sub>· traffic-fsm · usart-control</sub>

We will include the measured intervals and a terminal log of commands and replies in the evaluation. <sub>· traffic-fsm · model-gpio · usart-control</sub>

## Learning objectives

- Configure GPIO in STM32CubeMX, read an IR sensor and debounce a push button. <sub>· model-gpio</sub>
- Wire common-cathode RGB LEDs and verify the output mapping by checking the displayed colours. <sub>· model-gpio</sub>
- Design a state machine with non-blocking timing and test the permitted transitions. <sub>· traffic-fsm</sub>
- Exchange commands and replies through USART and verify that communication does not disrupt phase timing. <sub>· usart-control</sub>
- Define interfaces between C modules and verify their integration on the shared model. <sub>· traffic-fsm · model-gpio · usart-control</sub>

## Repository workflow

1. We keep the shared project version on `main`. Miroslav prepares changes to the `.ioc` configuration and GPIO mapping after discussing them with Branislav and Samuel. <sub>· traffic-fsm · model-gpio · usart-control</sub>
2. Each member develops and tests their module on their own branch. A teammate reviews each pull request before it is merged into `main`. <sub>· traffic-fsm · model-gpio · usart-control</sub>
3. After merging the modules, we verify their integration in the [integrated test on the assembled model](#integrated-test-on-the-assembled-model). <sub>· traffic-fsm · model-gpio · usart-control</sub>
4. Before the project demonstration, we build the firmware from `main`, flash it through ST-LINK and run the tests on the assembled model. We save the results with the wiring map and a sample terminal session. <sub>· traffic-fsm · model-gpio · usart-control</sub>
