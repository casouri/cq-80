SoftwareTimer blinkTimer;

void setup()
{
  blinkTimer.begin(1000, blinkRoutine);
  blinkTimer.start();
  suspendLoop();
}

void loop() {}

void blinkRoutine(TimerHandle_t _handle)
{
  /* Timer examples: https://github.com/adafruit/Adafruit_nRF52_Arduino/pull/262/commits/63b43fc2ec1650398fdc0e104479d85706af844a */
  digitalToggle(19);
}
