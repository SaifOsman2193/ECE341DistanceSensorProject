#include <Arduino.h>
#include <esp_arduino_version.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
#define BUZZER_PIN    25   // Hardware PWM on GPIO 25 (via BJT)
#define TRIG_PIN      18   // HC-SR04 TRIG on GPIO 18
#define ECHO_PIN      19   // HC-SR04 ECHO on GPIO 19 (via voltage divider)

// Maximum range setting (5 meters = 500 cm)
const float MAX_DISTANCE_CM = 500.0;
const float MIN_DISTANCE_CM = 8.0;

// ============================================================================
// HARDWARE PWM (LEDC) FUNCTIONS
// ============================================================================
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  // Modern ESP32 Core 3.x+ API
  void buzzerInit() {
    ledcAttach(BUZZER_PIN, 1000, 8); // 8-bit resolution
  }

  void buzzerTone(uint32_t freq) {
    if (freq == 0) {
      ledcWrite(BUZZER_PIN, 0);
    } else {
      ledcWriteTone(BUZZER_PIN, freq);
    }
  }

  void buzzerStop() {
    ledcWrite(BUZZER_PIN, 0);
  }
#else
  // Legacy ESP32 Core 2.x API
  #define PWM_CHANNEL    0
  #define PWM_RESOLUTION 8

  void buzzerInit() {
    ledcSetup(PWM_CHANNEL, 1000, PWM_RESOLUTION);
    ledcAttachPin(BUZZER_PIN, PWM_CHANNEL);
  }

  void buzzerTone(uint32_t freq) {
    if (freq == 0) {
      ledcWrite(PWM_CHANNEL, 0);
    } else {
      ledcWriteTone(PWM_CHANNEL, freq);
    }
  }

  void buzzerStop() {
    ledcWrite(PWM_CHANNEL, 0);
  }
#endif

// ============================================================================
// ULTRASONIC SENSOR READ FUNCTION (Up to 5+ Meters)
// ============================================================================
float readDistanceCM() {
  // Clear trigger pin
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Send 10 microsecond pulse to trigger ultrasonic burst
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // 35,000 µs timeout allows detecting obstacles up to ~6.0 meters
  long durationUs = pulseIn(ECHO_PIN, HIGH, 35000);

  if (durationUs == 0) {
    return -1.0; // Out of range or no echo detected
  }

  // Distance in cm = (duration / 2) * speed of sound (0.0343 cm/µs)
  return (durationUs * 0.0343) / 2.0;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=======================================================");
  Serial.println(" ESP32 Proximity Radar Alarm (0.08m to 5.0m Range)     ");
  Serial.println(" TRIG: GPIO 18 | ECHO: GPIO 19 | BUZZER: GPIO 25      ");
  Serial.println("=======================================================");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Initialize hardware PWM on buzzer
  buzzerInit();
  buzzerStop();
}

void loop() {
  float distanceCm = readDistanceCM();

  if (distanceCm < 0 || distanceCm > MAX_DISTANCE_CM) {
    // Beyond 5 meters / No obstacle detected:
    // Slow beacon pulse at 500 Hz every 1.5 seconds
    Serial.println("Distance: > 5.00 m (Out of Range / Clear)");
    buzzerTone(500);
    delay(40);
    buzzerStop();
    delay(1500);
  } 
  else if (distanceCm <= MIN_DISTANCE_CM) {
    // Critical proximity (< 8 cm): Continuous solid 3000 Hz screeching alarm!
    float distanceM = distanceCm / 100.0;
    Serial.printf("Distance: %.2f m (%.1f cm) -> [CRITICAL DANGER: SOLID ALARM]\n", distanceM, distanceCm);

    buzzerTone(3000); // Continuous tone
    delay(100);
  } 
  else {
    // Across the entire 8 cm to 500 cm (0.08m to 5.0m) range:
    // As the object gets closer:
    // 1. Pitch rises smoothly: 600 Hz (at 5m) -> 2700 Hz (at 8cm)
    // 2. Beep pause shrinks: 1000 ms (at 5m) -> 25 ms (at 8cm)
    uint32_t freq = map((long)distanceCm, (long)MIN_DISTANCE_CM, (long)MAX_DISTANCE_CM, 2700, 600);
    uint32_t beepDelay = map((long)distanceCm, (long)MIN_DISTANCE_CM, (long)MAX_DISTANCE_CM, 25, 1000);

    float distanceM = distanceCm / 100.0;
    Serial.printf("Distance: %.2f m (%5.1f cm) | Pitch: %4u Hz | Interval: %4u ms\n", 
                  distanceM, distanceCm, freq, beepDelay);

    // Active beep pulse
    buzzerTone(freq);
    delay(50); // 50ms active beep pulse

    // Silence pause between beeps
    buzzerStop();
    delay(beepDelay);
  }
}
