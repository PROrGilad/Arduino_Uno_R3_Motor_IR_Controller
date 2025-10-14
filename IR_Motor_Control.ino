#include <IRremote.hpp>

// --- Pins ---
const uint8_t IR_RECEIVE_PIN = 11; // IR 1838B OUT
const uint8_t IN1 = 9;             // PWM (Timer1 -> ~31kHz)
const uint8_t IN2 = 6;             // Direction (LOW = forward)
const uint8_t BUZZER_PIN = 4;      // Passive 2-pin buzzer (+) -> D4, (-) -> GND

// --- Speed levels ---
// 0 = Off, 1 = Low, 2 = Mid (now fastest), 3 = High (a bit slower than Mid)
uint8_t speedLevel = 0;
const uint8_t speeds[] = {0, 170, 255, 210};  // <-- swapped 2 & 3

// --- Smooth ramp ---
uint8_t  currentPWM = 0;
uint8_t  targetPWM  = 0;
const uint8_t  RAMP_STEP = 3;
const uint16_t RAMP_MS   = 3;
unsigned long lastRampMs = 0;

// --- Press handling ---
bool waitingRelease = false;
unsigned long lastRxMs = 0;
const unsigned long RELEASE_GAP_MS = 450;
unsigned long nextAllowedStepAt = 0;
const unsigned long COOLDOWN_MS = 700;

// --- Software beep (quiet, no timers used) ---
bool          buzzerActive   = false;
unsigned long buzzerStopAtMs = 0;
unsigned long nextToggleUs   = 0;
bool          buzzerState    = false;
const uint16_t BEEP_FREQ_HZ  = 600;
const uint16_t BEEP_MS       = 40;

void startQuietBeep() {
  pinMode(BUZZER_PIN, OUTPUT);
  buzzerActive   = true;
  buzzerState    = false;
  digitalWrite(BUZZER_PIN, LOW);
  buzzerStopAtMs = millis() + BEEP_MS;
  const unsigned long halfPeriodUs = (1000000UL / BEEP_FREQ_HZ) / 2UL;
  nextToggleUs = micros() + halfPeriodUs;
}

void serviceQuietBeep() {
  if (!buzzerActive) return;
  if (millis() >= buzzerStopAtMs) {
    buzzerActive = false;
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }
  const unsigned long nowUs = micros();
  if ((long)(nowUs - nextToggleUs) >= 0) {
    buzzerState = !buzzerState;
    digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
    const unsigned long halfPeriodUs = (1000000UL / BEEP_FREQ_HZ) / 2UL;
    nextToggleUs += halfPeriodUs;
  }
}

void setup() {
  Serial.begin(115200);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(IN2, LOW);
  analogWrite(IN1, 0);
  digitalWrite(BUZZER_PIN, LOW);

  // 31 kHz PWM on D9/D10
  TCCR1B = (TCCR1B & 0b11111000) | 0x01;

  Serial.println(F("Any button: Off → Low → Mid(255) → High(210) → Off"));
}

void loop() {
  const unsigned long now = millis();

  if (waitingRelease && (now - lastRxMs > RELEASE_GAP_MS))
    waitingRelease = false;

  if (IrReceiver.decode()) {
    lastRxMs = now;
    const bool isRepeat = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;

    if (!waitingRelease && !isRepeat && now >= nextAllowedStepAt) {
      speedLevel = (speedLevel + 1) % 4;   // Off→Low→Mid→High→Off
      targetPWM  = speeds[speedLevel];

      startQuietBeep();

      waitingRelease    = true;
      nextAllowedStepAt = now + COOLDOWN_MS;

      Serial.print(F("Speed level: ")); Serial.print(speedLevel);
      Serial.print(F("  Target PWM: ")); Serial.println(targetPWM);
    }
    IrReceiver.resume();
  }

  serviceQuietBeep();

  if (now - lastRampMs >= RAMP_MS) {
    lastRampMs = now;
    if (currentPWM < targetPWM) {
      currentPWM = constrain(currentPWM + RAMP_STEP, 0, targetPWM);
    } else if (currentPWM > targetPWM) {
      currentPWM = constrain(currentPWM - RAMP_STEP, targetPWM, 255);
    }
    analogWrite(IN1, currentPWM);
  }
}
