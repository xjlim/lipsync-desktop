
#include "settings.h"
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(INTERVAL);
  digitalWrite(LED_BUILTIN, LOW);
  delay(INTERVAL);
}
