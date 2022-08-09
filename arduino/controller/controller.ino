/* This is the program for music controller functionality.

Actions:
 - Press joystick: toggle head and side light.
 - Press and hold joystick for 1s: If not paired, start pairing, top
   light blinks fast for 30 seconds (fast mode), then slow for 90s
   (slow mode).
 - Tilt up:
 - Tilt down: play/pause
 - Tilt left: previous track
 - Tilt right: next track
 - Rotate clockwise: volume up
 - Rotate counter-clockwise: volume down

The program also monitors battery voltage, and when it dips below
3.3V, front lamp starts blinking, when the voltage rises above 4.1V,
blinking stops.

The main loop runs every 100ms. This interval is long enough to save
battery life and short enough to be responsive.

When connection breaks, the program automatically starts re-pairing
without blinking the top light. */

#include <bluefruit.h>

/* Joystick Position. */
enum JoyPos {
  JoyPosUp,
  JoyPosDown,
  JoyPosLeft,
  JoyPosRight,
  JoyPosCenter,
};

/* Controller event. */
enum Event {
  EventTiltDown,
  EventTiltUp,
  EventTiltLeft,
  EventTiltRight,
  EventClockwise,
  EventCounterClockwise,
  EventClick,
  EventClickHold,
  EventNull,
};

enum ClickState {
  ClickState0,
  ClickStatePressed,
  ClickStateHoldDetected,
};

enum LightState {
  LightStateOff,
  LightStateSide,
  LightStateHead,
  LightStateBoth,
};

enum LayoutMode {
  LayoutModeMusic,
  LayoutModeStudy,
};

const int mediaPlayPause = 0xCD;
const int mediaNext = 0xB5;
const int mediaPrevious = 0xB6;
const int mediaVolumeUp = 0xE9;
const int mediaVolumeDown = 0xEA;
const int mediaRandom = 0xB9;
const int mediaSeekForward = 0xC0;
const int mediaSeekBackward = 0xC1;
LayoutMode currentMode = LayoutModeMusic;

const int led1Pin = 17;
const int led2Pin = 19;

/* Pin for joystick x-axis. */
const int xPin = 2;
/* Pin for joystick y-axis. */
const int yPin = 3;
/* Pin for clicking the joystick. */
const int clickPin = 27;
unsigned long clickPressStart = 0;
ClickState clickState = ClickState0;
/* The max reading from an axis. */
const int joyMaxRange = 946;
const int joyRange = joyMaxRange / 2;
/* Reading below this is considered being in center region. */
const int joyCenterRange = joyRange / 2;
/* Last position we read. */
JoyPos lastPos = JoyPosCenter;
/* Last three positions we read. */
JoyPos posHistory[3] = {JoyPosCenter, JoyPosCenter, JoyPosCenter};

/* Bitmaps for controlling each light, in the order of top light, front
   lamp, head light, side lights. */
/* The time when blinking stated. */
const int lightPin[4] = { 16, 15, 7, 11 };
LightState lightState = LightStateOff;

/* We connect USB to A2 to detect if usb port is plugged in. */
const int usbPin = 4;
/* This pin is used to measure battery voltage. */
const int batteryPin = 31;

/* BLE device information object. */
BLEDis bledis;
/* BLE keyboard object. */
BLEHidAdafruit blehid;
const int advertizeFastModeTimeout = 30;
const int advertizeTimeout = 120;

/* https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/527e62d5f480c3f7f529bba06d88cd165de14626/cores/nRF5/utility/SoftwareTimer.h */
SoftwareTimer frontLampBlinkTimer;
SoftwareTimer checkBatteryTimer;
SoftwareTimer mainLoopTimer;
SoftwareTimer topLightBlinkTimer;
unsigned long topLightBlinkStart;

void setup()
{
  /* This seems to save some power. */
  NRF_POWER->DCDCEN = 1;
  /* Serial.begin(115200); */

  pinMode(led1Pin, OUTPUT);
  pinMode(led2Pin, OUTPUT);
  pinMode(lightPin[0], OUTPUT);
  pinMode(lightPin[1], OUTPUT);
  pinMode(lightPin[2], OUTPUT);
  pinMode(lightPin[3], OUTPUT);
  pinMode(clickPin, INPUT_PULLUP);
  digitalWrite(lightPin[0], LOW);
  digitalWrite(lightPin[1], LOW);
  digitalWrite(lightPin[2], LOW);
  digitalWrite(lightPin[3], LOW);

  Bluefruit.begin();
  /* Turn off the blue led that lights up when auto connection is
     on. */
  Bluefruit.autoConnLed(false);
  Bluefruit.setTxPower(4); // 4dBm, 2.5mW, 10m range.
  Bluefruit.setName("CQ-80");
  Bluefruit.setAppearance(BLE_APPEARANCE_GENERIC_REMOTE_CONTROL);
  /* Set connection interval (min, max) to 30ms–60ms. (Unit is
     1.25ms). Apple has specific requirements, see advertize().
  */
  Bluefruit.Periph.setConnInterval(24, 48);

  bledis.setManufacturer("Deepsea Metro Inc.");
  bledis.setModel("CQ-80");
  bledis.begin();

  blehid.begin();
  /* advertize(); */

  frontLampBlinkTimer.begin(2000, frontLampBlinkRoutine);
  frontLampBlinkTimer.start();
  checkBatteryTimer.begin(1000, checkBatteryRoutine);
  checkBatteryTimer.start();
  topLightBlinkTimer.begin(600, topLightBlinkRoutine);
  mainLoopTimer.begin(100, mainLoop);
  mainLoopTimer.start();
  suspendLoop();
}

void loop() {}

void mainLoop(TimerHandle_t _handle)
{
  /* Process click. */
  switch (detectClickEvent())
    {
    case EventClick:
      if (Bluefruit.connected())
        {
          flashTopLight(random(1,7));
        }
      break;
    case EventClickHold:
      if (!Bluefruit.connected())
        {
          Bluefruit.Advertising.start(advertizeTimeout);
          topLightBlinkTimer.setPeriod(300);
          topLightBlinkStart = millis();
          topLightBlinkTimer.start();
        }
      break;
    }

  if (Bluefruit.connected())
    {
      /* Process joystick. */
      int x = analogRead(xPin) - joyRange;
      int y = analogRead(yPin) - joyRange;
      JoyPos pos = readJoyPos(x, y, joyCenterRange);
      if (pos != JoyPosCenter)
        digitalWrite(lightPin[0], HIGH);
      else
        digitalWrite(lightPin[0], LOW);
      if (pos != lastPos)
        {
          pushNewPos(pos, posHistory);
          Event event = detectEvent(posHistory);
          if (event == EventTiltUp)
            {
              toggleLight();
            }
          else {
            int mediaCode = eventToMediaCode(event, currentMode);
            if (mediaCode != 0)
              {
                /* Send media key. */
                blehid.consumerKeyPress(mediaCode);
                delay(5);
                blehid.consumerKeyRelease();
              }
          }
          lastPos = pos;
        }
    }
  delay(100);
}

/* Return the appropriate consumer key given the event. Returns 0 for
   EventNull. */
int eventToMediaCode(Event event, LayoutMode mode)
{
  switch (mode)
    {
    case LayoutModeMusic:
      switch (event)
        {
        case EventTiltDown:
          return mediaPlayPause;
        case EventTiltLeft:
          return mediaPrevious;
        case EventTiltRight:
          return mediaNext;
        case EventClockwise:
          return mediaVolumeUp;
        case EventCounterClockwise:
          return mediaVolumeDown;
        case EventNull:
          return 0;
        default:
          return 0;
        }
    case LayoutModeStudy:
      switch (event)
        {
        case EventTiltDown:
          return mediaPlayPause;
        case EventTiltLeft:
          return mediaSeekBackward;
        case EventTiltRight:
          return mediaSeekForward;
        case EventClockwise:
          return mediaVolumeUp;
        case EventCounterClockwise:
          return mediaVolumeDown;
        case EventNull:
          return 0;
        default:
          return 0;
        }
    default:
      return 0;
    }
}

/*** Joystick */

/* Read a pos from x and y coordinates.
 centerRange is the range defined to be the center. */
JoyPos readJoyPos(int x, int y, int centerRange)
{
  if (abs(x) < centerRange && abs(y) < centerRange)
    return JoyPosCenter;

  if (abs(x) > abs(y))
    {
      if (x < 0)
        return JoyPosLeft;
      else
        return JoyPosRight;
    }
  else
    {
      if (y < 0)
        return JoyPosDown;
      else
        return JoyPosUp;
    }
}

/* Push a new pos into position history. */
void pushNewPos(JoyPos pos, JoyPos posHistory[3])
{
  posHistory[0] = posHistory[1];
  posHistory[1] = posHistory[2];
  posHistory[2] = pos;
}

/* Figure out the proper event from past joystick positions. */
Event detectEvent(JoyPos posHistory[3])
{
  /* We use three history positions so we can distinguish between tile
     and rotation. */
  JoyPos p0 = posHistory[0];
  JoyPos p1 = posHistory[1];
  JoyPos p2 = posHistory[2];

  /* Tilt left. */
  if (p0 == JoyPosCenter && p1 == JoyPosLeft && p2 == JoyPosCenter)
    return EventTiltLeft;
  /* Tilt right. */
  if (p0 == JoyPosCenter && p1 == JoyPosRight && p2 == JoyPosCenter)
    return EventTiltRight;
  /* Tilt up. */
  if (p0 == JoyPosCenter && p1 == JoyPosUp && p2 == JoyPosCenter)
    return EventTiltUp;
  /* Tilt down. */
  if (p0 == JoyPosCenter && p1 == JoyPosDown && p2 == JoyPosCenter)
    return EventTiltDown;
  /* Clockwise rotation. */
  if (p1 == JoyPosLeft && p2 == JoyPosUp
      || p1 == JoyPosUp && p2 == JoyPosRight
      || p1 == JoyPosRight && p2 == JoyPosDown
      || p1 == JoyPosDown && p2 == JoyPosLeft)
    return EventClockwise;
  /* Counter-clockwise rotation. */
  if (p2 == JoyPosLeft && p1 == JoyPosUp
      || p2 == JoyPosUp && p1 == JoyPosRight
      || p2 == JoyPosRight && p1 == JoyPosDown
      || p2 == JoyPosDown && p1 == JoyPosLeft)
    return EventCounterClockwise;
  return EventNull;
}

/* Returns EventClick, EventClickHold, or EventNull. Mutates
   clickState, ClickPressStart, reads clickPin. */
Event detectClickEvent(void)
{
  switch (clickState)
    {
    case ClickState0:
      if (digitalRead(clickPin) == LOW)
        {
          clickState = ClickStatePressed;
          clickPressStart = millis();
        }
      return EventNull;
    case ClickStatePressed:
      if (digitalRead(clickPin) == HIGH)
        {
          clickState = ClickState0;
          return EventClick;
        }
      if (millis() - clickPressStart > 1000)
        {
          clickState = ClickStateHoldDetected;
          return EventClickHold;
        }
      /* At this point, we are still holding but didn’t reach 1s,
         return null event. */
      return EventNull;
    case ClickStateHoldDetected:
      /* Once we detect and sent a hold event, we don’t send further
         events whether the joystick is released or not. */
      if (digitalRead(clickPin) == HIGH)
        {
          clickState = ClickState0;
        }
      return EventNull;
    default:
      return EventNull;
    }
}

/*** BLE */

void advertize(void)
{
  /* Define advertising packet. */
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  /* Include BLE HID service. */
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.ScanResponse.addName();
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 1285 ms.
   * - Timeout for fast mode is 30 seconds
   Apple has very specific interval requirements:
   https://developer.apple.com/library/archive/qa/qa1931/_index.html
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  /* Unit is 0.625ms. */
  Bluefruit.Advertising.setInterval(32, 2056);
  /* Set number of seconds in fast mode. */
  Bluefruit.Advertising.setFastTimeout(advertizeFastModeTimeout);
  /* Clean up after advertising stops. */
  Bluefruit.Advertising.setStopCallback(advertizeStopCallback);
  /* Start advertising and stop after n seconds, 0 = never stop. */
  Bluefruit.Advertising.start(advertizeTimeout);
}

void advertizeStopCallback(void)
{
  /* Turn off blinking. */
  topLightBlinkTimer.stop();
  digitalWrite(lightPin[0], LOW);
}

float readBatteryVoltage(void)
{
  /* REF: https://learn.adafruit.com/bluefruit-nrf52-feather-learning-guide/nrf52-adc */
  /* Default ADC range is 3.6V, resolution 10 bits. */
  const float vbatScale = 3600.0 / 1024.0;
  /* 2M + 0.806M voltage divider on VBAT = (2M / (0.806M + 2M)) =
     0.71275837F, the inverse of that is 1.403F. */
  const float vbatCompensation = 1.403F;
  /* Hopefully compiler can compile these away? */
  return analogRead(31) * (vbatScale * vbatCompensation);
}

void frontLampBlinkRoutine(TimerHandle_t _handle)
{
  /* Timer examples: https://github.com/adafruit/Adafruit_nRF52_Arduino/pull/262/commits/63b43fc2ec1650398fdc0e104479d85706af844a */
  digitalToggle(lightPin[1]);
}

/* Blink front lamp when voltage drops below 3.3V (below 10%) and
   stops blinking when voltages raises above 4.1V (almost full). */
void checkBatteryRoutine(TimerHandle_t _handle)
{
  float batteryVoltage = readBatteryVoltage();
  float usbVoltage = analogRead(usbPin);
  if (usbVoltage > 900) /* Magic number. */
    {
      frontLampBlinkTimer.stop();
      digitalWrite(lightPin[1], HIGH);
      return;
    }
  if (batteryVoltage < 3300)
    {
      frontLampBlinkTimer.start();
      return;
    }
  if (batteryVoltage > 4100)
    {
      frontLampBlinkTimer.stop();
      /* We don’t return after this check because we still want to run
         the next check. */
    }
  if (usbVoltage < 800)
    {
      digitalWrite(lightPin[1], LOW);
    }
}

/* Blink top light, if fast mode is past, blink slower, if advertize
   timeout is past, stop blinking. */
void topLightBlinkRoutine(TimerHandle_t _handle)
{
  digitalToggle(lightPin[0]);
  unsigned long currentTime = millis();
  if (currentTime - topLightBlinkStart
      > advertizeFastModeTimeout * 1000)
    {
      topLightBlinkTimer.setPeriod(1000);
    }
  if (currentTime - topLightBlinkStart > advertizeTimeout * 1000)
    {
      digitalWrite(lightPin[0], LOW);
      topLightBlinkTimer.stop();
    }
}

/* Toggle between each light layout. */
void toggleLight(void)
{
  switch (lightState)
    {
    case LightStateOff:
      lightState = LightStateSide;
      digitalWrite(lightPin[2], LOW);
      digitalWrite(lightPin[3], HIGH);
      break;
    case LightStateSide:
      lightState = LightStateHead;
      digitalWrite(lightPin[2], HIGH);
      digitalWrite(lightPin[3], LOW);
      break;
    case LightStateHead:
      lightState = LightStateBoth;
      digitalWrite(lightPin[2], HIGH);
      digitalWrite(lightPin[3], HIGH);
      break;
    case LightStateBoth:
      lightState = LightStateOff;
      digitalWrite(lightPin[2], LOW);
      digitalWrite(lightPin[3], LOW);
      break;
    }
}

void flashTopLight(int n)
{
  for (int count=0; count < n; count++)
    {
      digitalWrite(lightPin[0], HIGH);
      delay(100);
      digitalWrite(lightPin[0], LOW);
      delay(100);
    }
}
