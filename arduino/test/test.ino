/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/
#include <bluefruit.h>

#define POS_DETECTOR_THRESHOLD 3

BLEDis bledis;
BLEHidAdafruit blehid;
bool hasKeyPressed = false;

void setup()
{
  Serial.begin(115200);

  Serial.println("Start");

  Bluefruit.begin();
  Bluefruit.setTxPower(4); // 4dBm, 2.5mW, 10m range.
  Bluefruit.setName("CQ-80");

  // Configure and start Device Information Service.
  bledis.setManufacturer("Deepsea Metro Inc.");
  bledis.setModel("CQ-80");
  bledis.begin();

  /* Start BLE HID Note: Apple requires BLE device must have min
   * connection interval >= 20m (The smaller the connection interval
   * the faster we could send data). However for HID and MIDI device,
   * Apple could accept min connection interval up to 11.25 ms.
   * Therefore BLEHidAdafruit::begin() will try to set the min and max
   * connection interval to 11.25 ms and 15 ms respectively for best
   * performance.
   */
  blehid.begin();

  /* Set connection interval (min, max) to 20–30ms. (Unit is 1.25ms,
   * 16 * 1.25 = 20). Note: It is already set by
   * BLEHidAdafruit::begin() to 11.25ms–15ms.
   */
  Bluefruit.Periph.setConnInterval(16, 24);

  // Set up and start advertising.
  advertize();
}

void advertize(void)
{
  // Define advertising packet.
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);

  // Include BLE HID service.
  Bluefruit.Advertising.addService(blehid);

  // There isn't enough room in the advertising packet for the
  // name so we'll place it on the secondary Scan Response packet
  Bluefruit.ScanResponse.addName();

  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   *
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); // in unit of 0.625 ms
  // Set number of seconds in fast mode.
  Bluefruit.Advertising.setFastTimeout(30);
  // Start advertising and stop after n seconds, 0 = never stop.
  Bluefruit.Advertising.start(0);
}

void loop()
{
  if (Bluefruit.connected())
    {
      pressConsumerKey(blehid, 205);
    }
  delay(3000);
}

/* Press and release usage_code. For whatever reason consumerReport function isn't very reliable. */
void pressConsumerKey(BLEHidAdafruit blehid, uint16_t usage_code)
{
  blehid.consumerKeyPress(usage_code);
  delay(40);
  blehid.consumerKeyRelease();
}

