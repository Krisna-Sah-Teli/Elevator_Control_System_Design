# Three-Floor Elevator Control System

An event-based elevator simulation for an ESP8266 NodeMCU, written as an
Arduino sketch. It models three floors with a finite state machine (FSM),
interrupt-driven button inputs, queued floor requests, and non-blocking
travel and door timers.

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



## Hardware and pin mapping

| Function | NodeMCU pin | ESP8266 GPIO |
| --- | --- | --- |
| Floor 1 LED | D0 | GPIO16 |
| Floor 2 LED | D7 | GPIO13 |
| Floor 3 LED | D8 | GPIO15 |
| Floor 1 button | D1 | GPIO5 |
| Floor 2 button | D2 | GPIO4 |
| Floor 3 button | D3 | GPIO0 |
| Emergency button | D6 | GPIO12 |
| CLEAR button | D5 | GPIO14 |
| Emergency indicator | `LED_BUILTIN` (D4) | GPIO2 |

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

| State | Purpose |
| --- | --- |
| `IDLE` | Waits for pending requests and starts a trip when one exists. |
| `UP` | Advances one floor after each `FLOOR_TRAVEL_TIME` interval. |
| `DOWN` | Decreases one floor after each `FLOOR_TRAVEL_TIME` interval. |
| `DOOR_OPEN` | Runs the door-open interval followed by the door-closing interval. |
| `EMERGENCY` | Lights the built-in LED, ignores floor-button events, and waits for CLEAR. |

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

1. Wire the buttons and LEDs according to the pin table. Buttons connect
    between their input pin and GND; use a current-limiting resistor with
    each external LED.
2. Install the ESP8266 board package in Arduino IDE and select the
    NodeMCU ESP8266 board that matches your hardware.
3. Open `elevator_multiple_inputs/elevator_multiple_inputs.ino`.
4. Select the connected board and port, then upload the sketch.
5. Open Serial Monitor at **115200 baud**.
6. The simulation starts at Floor 1. Press one or more floor buttons to
    queue requests; press the emergency button to enter emergency mode.
    Press and hold CLEAR until the emergency-cleared message appears,
    then release it.

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

