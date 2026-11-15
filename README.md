# 3-Floor Elevator FSM --- ESP8266 NodeMCU

An event-based, three-floor elevator simulation implemented in Arduino
C++ for the ESP8266 NodeMCU. The program uses a finite state machine
(FSM), interrupt-driven button events, a pending-request register, and
`millis()`-based timing.

## Features

-   Three selectable floors.
-   Multiple floor requests can be queued before or during travel.
-   Directional request scheduling:
    -   While moving UP, requests above the current floor are served
        first; requests below are considered when no requests remain
        above.
    -   While moving DOWN, requests below are served first; requests
        above are considered when no requests remain below.
    -   From IDLE, the nearest pending floor is selected. If the
        distances are equal, the upper floor is selected.
-   One-floor-at-a-time simulated travel.
-   Timed door-open and door-close phases.
-   Emergency input with priority over normal FSM operation.
-   Emergency indicator using the NodeMCU built-in LED.
-   CLEAR button to leave emergency mode.
-   Serial Monitor messages for state and event feedback.

> This is an educational simulation. It does not control a real elevator
> and does not implement certified elevator safety functions.

## Hardware and pin mapping

  Function                       NodeMCU pin   ESP8266 GPIO
  --------------------- -------------------- --------------
  Floor 1 LED                             D0         GPIO16
  Floor 2 LED                             D7         GPIO13
  Floor 3 LED                             D8         GPIO15
  Floor 1 button                          D1          GPIO5
  Floor 2 button                          D2          GPIO4
  Floor 3 button                          D3          GPIO0
  Emergency button                        D6         GPIO12
  CLEAR button                            D5         GPIO14
  Emergency indicator     `LED_BUILTIN` (D4)          GPIO2

### Button wiring

The button inputs are configured with `INPUT_PULLUP`. Connect each
button between its assigned input pin and GND:

-   Released: HIGH
-   Pressed: LOW

The floor and emergency buttons trigger on `FALLING` edges. The CLEAR
button is polled in the main program and does not use an interrupt.

### LED wiring

The three floor indicator LEDs are driven by their assigned output pins.
Use an appropriate series current-limiting resistor for each external
LED. The built-in LED is active LOW on the NodeMCU: LOW turns it on and
HIGH turns it off.

**ESP8266 boot-pin caution:** D3/GPIO0 and D8/GPIO15 are boot-strapping
pins. External circuitry must not force them to an invalid level during
reset or startup. Confirm the board starts reliably with the final
wiring.

## FSM states

  -----------------------------------------------------------------------
  State                               Purpose
  ----------------------------------- -----------------------------------
  `IDLE`                              Waits for pending requests and
                                      starts a trip when one exists.

  `UP`                                Advances one floor after each
                                      `FLOOR_TRAVEL_TIME` interval.

  `DOWN`                              Decreases one floor after each
                                      `FLOOR_TRAVEL_TIME` interval.

  `DOOR_OPEN`                         Holds the door-open phase, then
                                      performs a door-closing phase.

  `EMERGENCY`                         Overrides normal operation, lights
                                      the built-in LED, discards
                                      floor-button events received during
                                      emergency, and waits for CLEAR.
  -----------------------------------------------------------------------

## Request handling

The program uses:

``` cpp
bool floorRequested[4] = { false, false, false, false };
```

Index 0 is unused. Indexes 1, 2, and 3 represent the three floors. A
`true` value means that floor has a pending request. Since each floor
has one Boolean flag, repeated requests for the same floor are coalesced
rather than counted separately.

The floor-button ISRs set event flags. `processFloorEvents()` transfers
those events into the request register and uses button latches to avoid
repeated requests while a button remains held. Requests are accepted in
normal states, including while moving and while the door is operating.

When the elevator reaches a requested floor, `startDoorOpen()` begins
the door cycle. The current-floor request is cleared after the door-open
interval, as the door enters its closing phase.

## Timing

The attached source defines:

``` cpp
const unsigned long FLOOR_TRAVEL_TIME = 2000;
const unsigned long DOOR_OPEN_TIME    = 2000;
const unsigned long DOOR_CLOSE_TIME   = 1000;
```

All values are in milliseconds.

-   `FLOOR_TRAVEL_TIME`: simulated time to move one floor.
-   `DOOR_OPEN_TIME`: duration of the door-open phase.
-   `DOOR_CLOSE_TIME`: duration of the door-closing phase.

Movement and door timing use `millis()` rather than a long delay,
allowing the main loop to continue checking inputs and emergency events
during normal movement and door operation. The CLEAR-button routine does
contain short blocking delays for debounce and release handling.

## Emergency behaviour in this version

The emergency interrupt sets `emergencyPressed`. The main loop checks
emergency events before running the normal state handler. On entering
emergency mode, the code remembers the previous state. If the previous
state was `UP` or `DOWN`, it advances the simulated position by one
floor in that direction (within the floor limits), then displays that
floor. If the previous state was `IDLE` or `DOOR_OPEN`, it remains at
the current floor.

While in `EMERGENCY`, the main loop discards floor-button event flags
and returns before normal state handling. The pending request register
is not globally cleared. Pressing CLEAR turns off the emergency LED,
sets the target to the current floor, and returns the FSM to `IDLE`.

**Important interpretation:** This is the behaviour of the submitted
simulation code. It is not a safety-rated emergency-stop strategy for a
physical elevator.

## Setup and operation

1.  Wire the LEDs and buttons according to the pin table.
2.  Install the ESP8266 board package in the Arduino IDE and select the
    appropriate NodeMCU ESP8266 board.
3.  Open `elevator_multiple_inputs.ino`.
4.  Upload the sketch.
5.  Open Serial Monitor at **115200 baud**.
6.  The system starts at Floor 1 and lights the Floor 1 LED.
7.  Press one or more floor buttons. The requests are stored and
    serviced according to the scheduling rules.
8.  Press the emergency button to enter emergency mode.
9.  Press and hold CLEAR until the system reports that the emergency is
    cleared; release it to complete the operation.

## Important code sections

-   **Pin definitions:** hardware pin assignments.
-   **FSM states and variables:** current state, floor, target, and
    direction.
-   **Interrupt service routines:** capture button edges by setting
    event flags.
-   **`processFloorEvents()`:** handles floor button events and latches.
-   **`selectNextTarget()`:** chooses the next requested floor based on
    direction and distance.
-   **`startMoving()`:** starts the next trip or opens the door for a
    same-floor request.
-   **`handleUp()` / `handleDown()`:** perform timed one-floor movement
    and check for stops.
-   **`handleDoorOpen()`:** handles door-open and door-close timing.
-   **`handleEmergency()`:** enters emergency mode and updates the
    simulated floor position.
-   **`processClearButton()`:** clears emergency mode.
-   **`setup()` / `loop()`:** configure hardware and coordinate the FSM.

## Recommended review items

These are review recommendations; they are not changes made to the
attached sketch.

1.  Declare `emergencyPressed` as
    `volatile bool emergencyPressed = false;` because it is written by
    `emergencyISR()` and read in normal program context. The floor event
    flags are already declared `volatile`.
2.  Consider using a short critical section when copying and clearing
    ISR event flags, to avoid a race between the ISR setting a flag and
    the main loop clearing it.
3.  Consider debouncing the floor and emergency inputs in a deliberate
    way. The latches prevent repeated processing while a button remains
    held, but they are not a full time-based electrical debounce filter.
4.  Validate the use of D3/GPIO0 and D8/GPIO15 with the actual circuit
    during reset, because they are ESP8266 boot-strapping pins.
5.  For any real elevator, use certified controllers, safety circuits,
    braking, door interlocks, limit switches, redundant sensing, and
    applicable standards. A microcontroller demonstration like this must
    not be used as a real elevator controller.

## Project files

-   `elevator_multiple_inputs.ino` --- Arduino sketch.
-   `README.md` --- setup, pinout, behaviour, and code overview.
-   `Elevator_FSM_Project_Report.docx` / `.pdf` --- project report.

## License

No license was specified in the supplied source. Add a license before
redistributing the project if required.
