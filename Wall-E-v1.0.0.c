#include <Bluepad32.h>        // Zorgt dat de ESP32 met een Xbox controller kan praten
#include <ESP32Servo.h>       // Zorgt dat de ESP32 servo motoren kan bewegen

/* ============================================================
   SERVO PINNEN
   Hier zeggen we: welke draad zit waar
   ============================================================ */
#define ARM_LEFT_PIN   16     // Draad waar de linker arm servo op zit
#define ARM_RIGHT_PIN  17     // Draad waar de rechter arm servo op zit
#define EYE_LEFT_PIN   4      // Draad waar het linker oog op zit
#define EYE_RIGHT_PIN  15     // Draad waar het rechter oog op zit

Servo armLeft, armRight;     // Besturing voor de twee arm-servo’s
Servo eyeLeft, eyeRight;     // Besturing voor de twee oog-servo’s

/* ============================================================
   ARM STATUS
   Dit zijn geheugen-waardes
   De ESP32 onthoudt hiermee waar de armen staan
   ============================================================ */
float armLeftPos  = 90;      // Linker arm begint in het midden
float armRightPos = 90;      // Rechter arm begint in het midden

bool armLeftLocked  = false; // Linker arm mag bewegen (false = niet vast)
bool armRightLocked = false; // Rechter arm mag bewegen

int activeArm = 0;           // Welke arm luisteren we nu?
                             // 0 = geen, 1 = links, 2 = rechts

/* ============================================================
   OGEN
   Startpositie van de ogen
   ============================================================ */
float eyeLeftPos  = 90;      // Linker oog kijkt recht vooruit
float eyeRightPos = 90;      // Rechter oog kijkt recht vooruit

/* ============================================================
   RIJDEN – STEPPERMOTOREN
   Elke motor heeft 3 draden:
   - EN  = motor aan/uit
   - STEP = stapjes geven
   - DIR = richting bepalen
   ============================================================ */
// Rechter motor
#define RIGHT_EN_PIN    5     // Zet de rechter motor aan
#define RIGHT_STEP_PIN  18    // Geeft stapjes aan de motor
#define RIGHT_DIR_PIN   19    // Bepaalt linksom of rechtsom

// Linker motor
#define LEFT_EN_PIN     26
#define LEFT_STEP_PIN   25
#define LEFT_DIR_PIN    23

/* ============================================================
   NEK – STEPPERMOTOR
   Deze motor beweegt de nek omhoog en omlaag
   ============================================================ */
#define NEK_EN_PIN     14     // Zet de nek-motor aan
#define NEK_STEP_PIN   12     // Geeft stapjes aan de nek
#define NEK_DIR_PIN    13     // Bepaalt omhoog of omlaag

/* ============================================================
   XBOX CONTROLLER
   Hier bewaren we de controller
   ============================================================ */
GamepadPtr gamepad = nullptr; // Hier komt de controller in te staan

// Dit gebeurt als de controller verbinding maakt
void onConnectedGamepad(GamepadPtr gp) {
  gamepad = gp;               // We onthouden deze controller
  Serial.println("Controller verbonden!");
}

// Dit gebeurt als de controller wegvalt
void onDisconnectedGamepad(GamepadPtr gp) {
  if (gp == gamepad)          // Alleen loskoppelen als dit dezelfde controller is
    gamepad = nullptr;
  Serial.println("Controller losgekoppeld!");
}

/* ============================================================
   SETUP
   Dit gebeurt 1 keer als de ESP32 aan gaat
   ============================================================ */
void setup() {
  Serial.begin(115200);       // Zorgt dat we tekst kunnen zien op de computer

  // Servo’s koppelen aan hun draden
  armLeft.attach(ARM_LEFT_PIN);
  armRight.attach(ARM_RIGHT_PIN);
  eyeLeft.attach(EYE_LEFT_PIN);
  eyeRight.attach(EYE_RIGHT_PIN);

  // Vertel de ESP32: deze pinnen sturen motoren aan
  pinMode(RIGHT_EN_PIN, OUTPUT);
  pinMode(RIGHT_STEP_PIN, OUTPUT);
  pinMode(RIGHT_DIR_PIN, OUTPUT);

  pinMode(LEFT_EN_PIN, OUTPUT);
  pinMode(LEFT_STEP_PIN, OUTPUT);
  pinMode(LEFT_DIR_PIN, OUTPUT);

  // Motoren aanzetten (bij deze drivers is LOW = aan)
  digitalWrite(RIGHT_EN_PIN, LOW);
  digitalWrite(LEFT_EN_PIN, LOW);

  // Nek-motor instellen
  pinMode(NEK_EN_PIN, OUTPUT);
  pinMode(NEK_STEP_PIN, OUTPUT);
  pinMode(NEK_DIR_PIN, OUTPUT);
  digitalWrite(NEK_EN_PIN, LOW);   // Nek-motor aan

  // Xbox controller klaarzetten
  BP32.setup(&onConnectedGamepad, &onDisconnectedGamepad);
  BP32.forgetBluetoothKeys();      // Oude verbindingen wissen

  Serial.println("ESP32 klaar – pair je Xbox controller.");
}

/* ============================================================
   LOOP
   Dit blijft de hele tijd opnieuw draaien
   ============================================================ */
void loop() {
  BP32.update();              // Kijk of de controller iets doet

  // Alleen doorgaan als er echt een controller verbonden is
  if (gamepad && gamepad->isConnected()) {

    /* ================= ARMEN ================= */
    float LT = gamepad->brake();      // Linker trekker
    float RT = gamepad->throttle();   // Rechter trekker

    if (LT > 5) activeArm = 1;        // Linker arm gekozen
    if (RT > 5) activeArm = 2;        // Rechter arm gekozen

    // A-knop zet de gekozen arm vast
    if (gamepad->a()) {
      if (activeArm == 1) armLeftLocked = true;
      if (activeArm == 2) armRightLocked = true;
    }

    // B-knop maakt alle armen weer los
    if (gamepad->b()) {
      armLeftLocked  = false;
      armRightLocked = false;
    }

    // Beweeg de armen zolang ze niet vast staan
    if (!armLeftLocked && LT > 5)
      armLeftPos = map(LT, 0, 1023, 160, 20);

    if (!armRightLocked && RT > 5)
      armRightPos = map(RT, 0, 1023, 20, 160);

    // D-pad omlaag zet armen terug naar het midden
    if (gamepad->dpad() & DPAD_DOWN) {
      armLeftPos  = 90;
      armRightPos = 90;
    }

    /* ================= OGEN ================= */
    float eyeStep = 1.8;               // Hoe groot elke oogbeweging is

    if (gamepad->l1()) eyeLeftPos  -= eyeStep;  // Linker oog beweegt
    if (gamepad->r1()) eyeRightPos += eyeStep;  // Rechter oog beweegt

    // Grenzen zodat de ogen niet kapot gaan
    eyeLeftPos  = constrain(eyeLeftPos,  40, 140);
    eyeRightPos = constrain(eyeRightPos, 40, 140);

    // D-pad omhoog zet ogen recht vooruit
    if (gamepad->dpad() & DPAD_UP) {
      eyeLeftPos  = 90;
      eyeRightPos = 90;
    }

    /* ================= RIJDEN ================= */
    int joyY = gamepad->axisY();       // Joystick vooruit / achteruit
    int joyX = gamepad->axisX();       // Joystick links / rechts

    if (abs(joyY) < 40) joyY = 0;      // Kleine bewegingen negeren
    if (abs(joyX) < 40) joyX = 0;

    // Bereken hoeveel elke motor moet doen
    int rawLeft  = joyY + joyX;
    int rawRight = joyY - joyX;

    rawLeft  = constrain(rawLeft,  -512, 512);
    rawRight = constrain(rawRight, -512, 512);

    int leftCmd  = rawRight;           // Correcte motor links
    int rightCmd = rawLeft;            // Correcte motor rechts

    // Bepaal draairichting van de motoren
    digitalWrite(LEFT_DIR_PIN,  leftCmd  >= 0 ? LOW : HIGH);
    digitalWrite(RIGHT_DIR_PIN, rightCmd >= 0 ? LOW : HIGH);

    /* ================= NEK ================= */
    int neckJoy = gamepad->axisRY();   // Rechter joystick omhoog / omlaag

    // Richting van de nek bepalen
    digitalWrite(NEK_DIR_PIN, neckJoy >= 0 ? LOW : HIGH);
  }

  /* ================= SERVO’S ECHT BEWEGEN ================= */
  armLeft.write(armLeftPos);    // Zet linker arm naar de juiste positie
  armRight.write(armRightPos);  // Zet rechter arm naar de juiste positie
  eyeLeft.write(eyeLeftPos);    // Zet linker oog
  eyeRight.write(eyeRightPos);  // Zet rechter oog
}