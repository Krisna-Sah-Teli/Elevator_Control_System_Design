// ============================================
// 3-Floor Elevator FSM - ESP8266
// Event-Based Multi-Floor Selection
// ============================================


// ---------- Floor LEDs ----------
#define FLOOR1_LED D0
#define FLOOR2_LED D7
#define FLOOR3_LED D8


// ---------- Floor Buttons ----------
#define FLOOR1_BUTTON D1
#define FLOOR2_BUTTON D2
#define FLOOR3_BUTTON D3


// ---------- Emergency Buttons ----------
#define EMERGENCY_BUTTON D6
#define CLEAR_BUTTON     D5


// ---------- Built-in LED ----------
#define EMERGENCY_LED LED_BUILTIN



// ============================================
// FSM States
// ============================================

enum State {

  IDLE,
  UP,
  DOWN,
  DOOR_OPEN,
  EMERGENCY
};


State state = IDLE;



// ============================================
// Elevator Variables
// ============================================

int currentFloor = 1;
int targetFloor = 1;


// Direction used when deciding
// which pending request to serve next
State travelDirection = IDLE;


// Remember state before emergency
State previousState = IDLE;



// ============================================
// Floor Request Register
// ============================================
//
// Index 0 is unused.
//
// floorRequested[1] = Floor 1 requested
// floorRequested[2] = Floor 2 requested
// floorRequested[3] = Floor 3 requested
//
// This allows multiple floor requests
// to exist at the same time.
//

volatile bool floor1Event = false;
volatile bool floor2Event = false;
volatile bool floor3Event = false;


// Stored requests
bool floorRequested[4] = {
  false,
  false,
  false,
  false
};



// ============================================
// Button Latches
// ============================================
//
// Prevent one physically-held button from
// creating repeated requests.
//

bool floor1Latched = false;
bool floor2Latched = false;
bool floor3Latched = false;

bool emergencyPressed = false;
bool emergencyLatched = false;



// ============================================
// Timing Variables
// ============================================

unsigned long movementStartTime = 0;

const unsigned long FLOOR_TRAVEL_TIME = 2000;



// ============================================
// Door Timing
// ============================================

unsigned long doorTimer = 0;

bool doorClosing = false;

const unsigned long DOOR_OPEN_TIME  = 2000;
const unsigned long DOOR_CLOSE_TIME = 1000;



// ============================================
// Emergency Interrupt
// ============================================

void ICACHE_RAM_ATTR emergencyISR() {

  emergencyPressed = true;
}



// ============================================
// Floor 1 Interrupt
// ============================================

void ICACHE_RAM_ATTR floor1ISR() {

  floor1Event = true;
}



// ============================================
// Floor 2 Interrupt
// ============================================

void ICACHE_RAM_ATTR floor2ISR() {

  floor2Event = true;
}



// ============================================
// Floor 3 Interrupt
// ============================================

void ICACHE_RAM_ATTR floor3ISR() {

  floor3Event = true;
}



// ============================================
// Display Current Floor
// ============================================

void displayFloor() {

  // ------------------------------------------
  // Turn OFF all floor LEDs
  // ------------------------------------------

  digitalWrite(FLOOR1_LED, LOW);
  digitalWrite(FLOOR2_LED, LOW);
  digitalWrite(FLOOR3_LED, LOW);


  // ------------------------------------------
  // Turn ON current floor LED
  // ------------------------------------------

  if (currentFloor == 1) {

    digitalWrite(FLOOR1_LED, HIGH);
  }

  else if (currentFloor == 2) {

    digitalWrite(FLOOR2_LED, HIGH);
  }

  else if (currentFloor == 3) {

    digitalWrite(FLOOR3_LED, HIGH);
  }
}



// ============================================
// Get LED of Current Floor
// ============================================

int getFloorLED() {

  if (currentFloor == 1)
    return FLOOR1_LED;

  if (currentFloor == 2)
    return FLOOR2_LED;

  return FLOOR3_LED;
}



// ============================================
// Add Floor Request
// ============================================

void addFloorRequest(int floor) {

  if (floor < 1 || floor > 3)
    return;


  if (floorRequested[floor] != true){
    floorRequested[floor] = true;


    Serial.print("Floor ");
    Serial.print(floor);
    Serial.println(" selected.");
  }
}



// ============================================
// Process Floor Button Events
// ============================================
//
// Interrupts only set event flags.
//
// Actual processing happens here in the
// normal program context.
//

void processFloorEvents() {

  // ==========================================
  // FLOOR 1 EVENT
  // ==========================================

  if (floor1Event) {

    floor1Event = false;

    if (!floor1Latched) {

      floor1Latched = true;

      addFloorRequest(1);
    }
  }


  // Release latch when button released

  if (digitalRead(FLOOR1_BUTTON) == HIGH) {

    floor1Latched = false;
  }



  // ==========================================
  // FLOOR 2 EVENT
  // ==========================================

  if (floor2Event) {

    floor2Event = false;

    if (!floor2Latched) {

      floor2Latched = true;

      addFloorRequest(2);
    }
  }


  // Release latch when button released

  if (digitalRead(FLOOR2_BUTTON) == HIGH) {

    floor2Latched = false;
  }



  // ==========================================
  // FLOOR 3 EVENT
  // ==========================================

  if (floor3Event) {

    floor3Event = false;

    if (!floor3Latched) {

      floor3Latched = true;

      addFloorRequest(3);
    }
  }


  // Release latch when button released

  if (digitalRead(FLOOR3_BUTTON) == HIGH) {

    floor3Latched = false;
  }
}



// ============================================
// Check For Any Pending Request
// ============================================

bool hasPendingRequests() {

  if (floorRequested[1])
    return true;

  if (floorRequested[2])
    return true;

  if (floorRequested[3])
    return true;

  return false;
}



// ============================================
// Find Request Above
// ============================================

int findRequestAbove() {

  for (int floor = currentFloor + 1;
       floor <= 3;
       floor++) {

    if (floorRequested[floor]) {

      return floor;
    }
  }

  return 0;
}



// ============================================
// Find Request Below
// ============================================

int findRequestBelow() {

  for (int floor = currentFloor - 1;
       floor >= 1;
       floor--) {

    if (floorRequested[floor]) {

      return floor;
    }
  }

  return 0;
}



// ============================================
// Select Next Target
// ============================================
//
// Directional scheduling:
//
// UP:
//   First serve requests above.
//   If none remain, serve requests below.
//
// DOWN:
//   First serve requests below.
//   If none remain, serve requests above.
//
// IDLE:
//   Select nearest pending floor.
//

int selectNextTarget() {

  // ==========================================
  // Same-floor request
  // ==========================================

  if (floorRequested[currentFloor]) {

    return currentFloor;
  }



  // ==========================================
  // Currently moving UP
  // ==========================================

  if (travelDirection == UP) {

    int above = findRequestAbove();

    if (above != 0) {

      return above;
    }


    int below = findRequestBelow();

    if (below != 0) {

      return below;
    }
  }



  // ==========================================
  // Currently moving DOWN
  // ==========================================

  if (travelDirection == DOWN) {

    int below = findRequestBelow();

    if (below != 0) {

      return below;
    }


    int above = findRequestAbove();

    if (above != 0) {

      return above;
    }
  }



  // ==========================================
  // IDLE
  // ==========================================

  int above = findRequestAbove();
  int below = findRequestBelow();


  if (above != 0 && below != 0) {

    // Select nearest request

    if ((above - currentFloor) <=
        (currentFloor - below)) {

      return above;
    }

    else {

      return below;
    }
  }


  if (above != 0)
    return above;


  if (below != 0)
    return below;


  return 0;
}



// ============================================
// Start Door Open
// ============================================

void startDoorOpen() {

  Serial.println();
  Serial.println("==============================");
  Serial.print("DOOR OPEN - Floor ");
  Serial.println(currentFloor);
  Serial.println("==============================");



  // ------------------------------------------
  // Start door timer
  // ------------------------------------------

  doorTimer = millis();

  doorClosing = false;


  state = DOOR_OPEN;
}



// ============================================
// Start Moving
// ============================================

void startMoving() {

  // ------------------------------------------
  // Find next target
  // ------------------------------------------

  int nextFloor = selectNextTarget();


  // ------------------------------------------
  // No request
  // ------------------------------------------

  if (nextFloor == 0) {

    state = IDLE;

    travelDirection = IDLE;

    return;
  }


  targetFloor = nextFloor;



  // ==========================================
  // Same Floor
  // ==========================================

  if (targetFloor == currentFloor) {

    Serial.println();
    Serial.println("Already on this floor.");

    startDoorOpen();

    return;
  }



  // ==========================================
  // Move UP
  // ==========================================

  if (targetFloor > currentFloor) {

    travelDirection = UP;

    state = UP;

    movementStartTime = millis();


    Serial.println();
    Serial.println("Elevator moving UP ↑...");
    Serial.print("Current floor: ");
    Serial.println(currentFloor);

    return;
  }



  // ==========================================
  // Move DOWN
  // ==========================================

  if (targetFloor < currentFloor) {

    travelDirection = DOWN;

    state = DOWN;

    movementStartTime = millis();


    Serial.println();
    Serial.println("Elevator moving DOWN ↓...");
    Serial.print("Current floor: ");
    Serial.println(currentFloor);
    return;
  }
}



// ============================================
// Handle UP State
// ============================================

void handleUp() {

  // ------------------------------------------
  // Wait until one-floor travel time expires
  // ------------------------------------------

  if (millis() - movementStartTime <
      FLOOR_TRAVEL_TIME) {

    return;
  }


  // ------------------------------------------
  // One floor completed
  // ------------------------------------------

  movementStartTime = millis();


  if (currentFloor < 3) {

    currentFloor++;

    displayFloor();


    Serial.print("Current Floor: ");
    Serial.println(currentFloor);
  }



  // ==========================================
  // Requested Floor Reached
  // ==========================================

  if (floorRequested[currentFloor]) {

    Serial.println("Requested floor reached.");

    startDoorOpen();

    return;
  }



  // ==========================================
  // Continue / Change Direction
  // ==========================================

  int nextFloor = selectNextTarget();


  if (nextFloor == 0) {

    state = IDLE;

    travelDirection = IDLE;

    Serial.println("No pending requests.");
    Serial.println("Elevator waiting in IDLE.");

    return;
  }


  targetFloor = nextFloor;


  if (targetFloor < currentFloor) {

    travelDirection = DOWN;

    state = DOWN;

    movementStartTime = millis();

    Serial.println("Changing direction: DOWN");
  }
}



// ============================================
// Handle DOWN State
// ============================================

void handleDown() {

  // ------------------------------------------
  // Wait until one-floor travel time expires
  // ------------------------------------------

  if (millis() - movementStartTime <
      FLOOR_TRAVEL_TIME) {

    return;
  }


  // ------------------------------------------
  // One floor completed
  // ------------------------------------------

  movementStartTime = millis();


  if (currentFloor > 1) {

    currentFloor--;

    displayFloor();

    Serial.print("Current Floor: ");
    Serial.println(currentFloor);
  }



  // ==========================================
  // Requested Floor Reached
  // ==========================================

  if (floorRequested[currentFloor]) {

    Serial.println("Requested floor reached.");

    startDoorOpen();

    return;
  }



  // ==========================================
  // Continue / Change Direction
  // ==========================================

  int nextFloor = selectNextTarget();


  if (nextFloor == 0) {

    state = IDLE;

    travelDirection = IDLE;

    Serial.println("No pending requests.");
    Serial.println("Elevator waiting in IDLE.");

    return;
  }


  targetFloor = nextFloor;


  if (targetFloor > currentFloor) {

    travelDirection = UP;

    state = UP;

    movementStartTime = millis();

    Serial.println("Changing direction: UP");
  }
}



// ============================================
// Handle Door Open State
// ============================================

void handleDoorOpen() {

  // ==========================================
  // Door Open Period
  // ==========================================

  if (!doorClosing) {

    if (millis() - doorTimer >=
        DOOR_OPEN_TIME) {

      Serial.println("Door CLOSING...");

      // ------------------------------------------
      // Request at current floor has been served
      // ------------------------------------------

      floorRequested[currentFloor] = false;

      doorClosing = true;

      doorTimer = millis();
    }

    return;
  }



  // ==========================================
  // Door Closing Period
  // ==========================================

  if (millis() - doorTimer >=
      DOOR_CLOSE_TIME) {

    Serial.println("DOOR CLOSED!");


    // ----------------------------------------
    // Check for another pending request
    // ----------------------------------------

    if (hasPendingRequests()) {

      Serial.println();
      Serial.println("Pending request detected.");

      startMoving();
    }

    else {

      state = IDLE;

      travelDirection = IDLE;

      targetFloor = currentFloor;

      Serial.println("Elevator waiting in IDLE.");
    }
  }
}



// ============================================
// Emergency Handling
// ============================================

void handleEmergency() {

  // ------------------------------------------
  // Ignore if already in emergency
  // ------------------------------------------

  if (state == EMERGENCY) {

    return;
  }



  Serial.println();
  Serial.println("==============================");
  Serial.println("      EMERGENCY STOP!");
  Serial.println("==============================");


  // ------------------------------------------
  // Remember previous state
  // ------------------------------------------

  previousState = state;


  state = EMERGENCY;


  // ------------------------------------------
  // Emergency LED ON
  // ------------------------------------------

  digitalWrite(EMERGENCY_LED, LOW);



  // ==========================================
  // Emergency During UP
  // ==========================================

  if (previousState == UP) {

    Serial.println("Direction: UP");


    if (currentFloor < 3) {

      currentFloor++;
    }
  }



  // ==========================================
  // Emergency During DOWN
  // ==========================================

  else if (previousState == DOWN) {

    Serial.println("Direction: DOWN");


    if (currentFloor > 1) {

      currentFloor--;
    }
  }

  displayFloor();


  Serial.print("Emergency stopped at floor: ");
  Serial.println(currentFloor);


  Serial.println();
  Serial.println("Floor buttons DISABLED.");
  Serial.println("Press CLEAR button to resume.");
}



// ============================================
// Check Emergency Event
// ============================================

void processEmergencyEvent() {

  // ------------------------------------------
  // Emergency interrupt detected
  // ------------------------------------------

  if (emergencyPressed) {

    emergencyPressed = false;


    if (!emergencyLatched) {

      emergencyLatched = true;

      handleEmergency();
    }
  }


  // ------------------------------------------
  // Emergency button released
  // ------------------------------------------

  if (digitalRead(EMERGENCY_BUTTON) == HIGH) {

    emergencyLatched = false;
  }
}



// ============================================
// Handle Clear Button
// ============================================

void processClearButton() {

  // ------------------------------------------
  // CLEAR only works in EMERGENCY
  // ------------------------------------------

  if (state != EMERGENCY) {

    return;
  }


  if (digitalRead(CLEAR_BUTTON) == LOW) {

    delay(50);


    if (digitalRead(CLEAR_BUTTON) == LOW) {

      Serial.println();
      Serial.println("==============================");
      Serial.println("     EMERGENCY CLEARED");
      Serial.println("==============================");


      // ----------------------------------------
      // Turn OFF emergency LED
      // ----------------------------------------

      digitalWrite(EMERGENCY_LED, HIGH);


      // ----------------------------------------
      // Remain at current floor
      // ----------------------------------------

      targetFloor = currentFloor;


      // ----------------------------------------
      // Return to IDLE
      // ----------------------------------------

      state = IDLE;

      travelDirection = IDLE;


      Serial.print("Current Floor: ");
      Serial.println(currentFloor);

      Serial.println("Floor buttons ENABLED.");


      // ----------------------------------------
      // Wait for CLEAR release
      // ----------------------------------------

      while (digitalRead(CLEAR_BUTTON) == LOW) {

        delay(10);
      }


      delay(100);


      Serial.println("System Ready");
    }
  }
}



// ============================================
// Setup
// ============================================

void setup() {

  Serial.begin(115200);



  // ==========================================
  // Floor LEDs
  // ==========================================

  pinMode(FLOOR1_LED, OUTPUT);
  pinMode(FLOOR2_LED, OUTPUT);
  pinMode(FLOOR3_LED, OUTPUT);



  // ==========================================
  // Floor Buttons
  // ==========================================

  pinMode(FLOOR1_BUTTON, INPUT_PULLUP);
  pinMode(FLOOR2_BUTTON, INPUT_PULLUP);
  pinMode(FLOOR3_BUTTON, INPUT_PULLUP);



  // ==========================================
  // Emergency Buttons
  // ==========================================

  pinMode(EMERGENCY_BUTTON, INPUT_PULLUP);
  pinMode(CLEAR_BUTTON, INPUT_PULLUP);



  // ==========================================
  // Emergency LED
  // ==========================================

  pinMode(EMERGENCY_LED, OUTPUT);

  // Built-in LED OFF
  digitalWrite(EMERGENCY_LED, HIGH);



  // ==========================================
  // Floor Button Interrupts
  // ==========================================

  attachInterrupt(
    digitalPinToInterrupt(FLOOR1_BUTTON),
    floor1ISR,
    FALLING
  );


  attachInterrupt(
    digitalPinToInterrupt(FLOOR2_BUTTON),
    floor2ISR,
    FALLING
  );


  attachInterrupt(
    digitalPinToInterrupt(FLOOR3_BUTTON),
    floor3ISR,
    FALLING
  );



  // ==========================================
  // Emergency Interrupt
  // ==========================================

  attachInterrupt(
    digitalPinToInterrupt(EMERGENCY_BUTTON),
    emergencyISR,
    FALLING
  );



  // ==========================================
  // Start at Floor 1
  // ==========================================

  currentFloor = 1;

  targetFloor = 1;

  travelDirection = IDLE;


  displayFloor();



  // ==========================================
  // Startup Message
  // ==========================================

  Serial.println();
  Serial.println("==============================");
  Serial.println("      ELEVATOR SYSTEM");
  Serial.println("==============================");

  Serial.println("Current Floor: 1");
  Serial.println("System Ready");
}



// ============================================
// Main Loop
// ============================================

void loop() {


  // ==========================================
  // 1. PROCESS FLOOR INPUT EVENTS
  // ==========================================
  //
  // These are processed in EVERY normal state.
  //
  // Therefore:
  //
  // IDLE       -> accepts floor requests
  // UP         -> accepts floor requests
  // DOWN       -> accepts floor requests
  // DOOR_OPEN  -> accepts floor requests
  // EMERGENCY  -> events are ignored
  //

  if (state != EMERGENCY) {

    processFloorEvents();
  }



  // ==========================================
  // 2. EMERGENCY HAS HIGHEST PRIORITY
  // ==========================================

  processEmergencyEvent();



  // ==========================================
  // 3. EMERGENCY STATE
  // ==========================================

  if (state == EMERGENCY) {

    // Discard all floor-selection events
    floor1Event = false;
    floor2Event = false;
    floor3Event = false;

    processClearButton();

    return;
  }



  // ==========================================
  // 4. IDLE STATE
  // ==========================================

  if (state == IDLE) {

    if (hasPendingRequests()) {

      startMoving();
    }
  }



  // ==========================================
  // 5. UP STATE
  // ==========================================

  else if (state == UP) {

    handleUp();
  }



  // ==========================================
  // 6. DOWN STATE
  // ==========================================

  else if (state == DOWN) {

    handleDown();
  }



  // ==========================================
  // 7. DOOR OPEN STATE
  // ==========================================

  else if (state == DOOR_OPEN) {

    handleDoorOpen();
  }
}