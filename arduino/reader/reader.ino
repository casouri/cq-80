/* Joystick Position. */
enum JoyPos {
  JoyPosUp,
  JoyPosDown,
  JoyPosLeft,
  JoyPosRight,
  JoyPosCenter,
};

enum Event {
  EventPlayPause,
  EventBack,
  EventForward,
  EventVolumeUp,
  EventVolumeDown,
  EventRandom,
  EventNull,
};

const int xPin = 2;
const int yPin = 3;
const int joyMaxRange = 946;
const int joyRange = joyMaxRange / 2;
const int joyCenterRange = joyRange / 2;
JoyPos lastPos = JoyPosCenter;
JoyPos posHistory[3] = {JoyPosCenter, JoyPosCenter, JoyPosCenter};

/* Default ADC range is 3.6V, resolution 10 bits. */
const float vbatScale = 3600.0 / 1024.0;
/* 2M + 0.806M voltage divider on VBAT = (2M / (0.806M + 2M)) =
   0.71275837F, the inverse of that is 1.403F. */
const float vbatCompensation = 1.403;

void setup()
{
  Serial.begin(115200);
  pinMode(31, INPUT);
  pinMode(4, INPUT);
}

void loop()
{
  float vbat = analogRead(31) * (vbatScale * vbatCompensation);
  float usb = analogRead(4);
  Serial.print("VBat: " ); Serial.println(vbat);
  Serial.print("USB: " ); Serial.println(usb);
  delay(1000);
}

/* void loop() */
/* { */
/*   int x = analogRead(xPin); */
/*   int y = analogRead(yPin); */
/*   Serial.print("x="); */
/*   Serial.print(x); */
/*   Serial.print(" y="); */
/*   Serial.println(y); */
/*   delay(50); */
/* } */

/* void loop () */
/* { */
/*   int x = analogRead(xPin) - joyRange; */
/*   int y = analogRead(yPin) - joyRange; */

/*   JoyPos pos = readJoyPos(x, y, joyCenterRange); */
/*   switch (pos) */
/*     { */
/*     case JoyPosUp: */
/*       Serial.println("Up"); */
/*       break; */
/*     case JoyPosDown: */
/*       Serial.println("Down"); */
/*       break; */
/*     case JoyPosLeft: */
/*       Serial.println("Left"); */
/*       break; */
/*     case JoyPosRight: */
/*       Serial.println("Right"); */
/*       break; */
/*     case JoyPosCenter: */
/*       Serial.println("Center"); */
/*       break; */
/*     } */

/*   delay(50); */
/* } */

/* void loop () */
/* { */
/*   int x = analogRead(xPin) - joyRange; */
/*   int y = analogRead(yPin) - joyRange; */

/*   JoyPos pos = readJoyPos(x, y, joyCenterRange); */
/*   if (pos != lastPos) */
/*     { */
/*       pushNewPos(pos, posHistory); */
/*       Event event = detectEvent(posHistory); */
/*       switch (event) */
/*         { */
/*         case EventPlayPause: */
/*           Serial.println("PlayPause"); */
/*           break; */
/*         case EventBack: */
/*           Serial.println("Back"); */
/*           break; */
/*         case EventForward: */
/*           Serial.println("Forward"); */
/*           break; */
/*         case EventVolumeUp: */
/*           Serial.println("Up"); */
/*           break; */
/*         case EventVolumeDown: */
/*           Serial.println("Down"); */
/*           break; */
/*         case EventRandom: */
/*           Serial.println("Random"); */
/*           break; */
/*         case EventNull: */
/*           break; */
/*         } */
/*       lastPos = pos; */
/*     } */
/*   delay(50); */
/* } */

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

void pushNewPos(JoyPos pos, JoyPos posHistory[3])
{
  posHistory[0] = posHistory[1];
  posHistory[1] = posHistory[2];
  posHistory[2] = pos;
}

Event detectEvent(JoyPos posHistory[3])
{
  JoyPos p0 = posHistory[0];
  JoyPos p1 = posHistory[1];
  JoyPos p2 = posHistory[2];

  if (p0 == JoyPosCenter && p1 == JoyPosLeft && p2 == JoyPosCenter)
    return EventBack;
  if (p0 == JoyPosCenter && p1 == JoyPosRight && p2 == JoyPosCenter)
    return EventForward;
  if (p0 == JoyPosCenter && p1 == JoyPosUp && p2 == JoyPosCenter)
    return EventRandom;
  if (p0 == JoyPosCenter && p1 == JoyPosDown && p2 == JoyPosCenter)
    return EventPlayPause;
  if (p1 == JoyPosLeft && p2 == JoyPosUp
      || p1 == JoyPosUp && p2 == JoyPosRight
      || p1 == JoyPosRight && p2 == JoyPosDown
      || p1 == JoyPosDown && p2 == JoyPosLeft)
    return EventVolumeUp;
  if (p2 == JoyPosLeft && p1 == JoyPosUp
      || p2 == JoyPosUp && p1 == JoyPosRight
      || p2 == JoyPosRight && p1 == JoyPosDown
      || p2 == JoyPosDown && p1 == JoyPosLeft)
    return EventVolumeDown;
  return EventNull;
}
