#include <EEPROM.h>
#include <Mouse.h>
#include <math.h>
#include "settings.h"

#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"
#include "bluefruitConfig.h"

// Changelog of changes
// Replaced com_mode with BLUETOOTH_ENABLED
// Changed Pin numbers and removed TRANS_CONTROL and PIO4 (No need for external BT chip)
// Need to do:
// 1. Scrolling
// 2. Sip/Puff
// 3. Different types of clicks
// 4. ble_mouse()

// ***************** BLUETOOTH *****************
#if SOFTWARE_SERIAL_AVAILABLE
  #include <SoftwareSerial.h>
#endif

// Create Bluetooth Object
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// Not sure what this does (But was in all BT example sketches in the Adafruit BT Examples)
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

// *********************************************

//***SETTINGS***//
#ifndef SPEED_COUNTER_SETTING
#define SPEED_COUNTER_SETTING 8 // Default 4
#endif

#ifndef SIP_THRESHOLD_SETTING
#define SIP_THRESHOLD_SETTING 0.5
#endif

#ifndef PUFF_THRESHOLD_SETTING
#define PUFF_THRESHOLD_SETTING 0.5
#endif

#ifndef SIPPUFF_SECONDARY_DURATION_THRESHOLD_SETTING
#define SIPPUFF_SECONDARY_DURATION_THRESHOLD_SETTING 1
#endif

#ifndef SIPPUFF_TERTIARY_DURATION_THRESHOLD_SETTING
#define SIPPUFF_TERTIARY_DURATION_THRESHOLD_SETTING 5
#endif

#ifndef SIP_PUFF_REVERSED
#define SIP_PUFF_REVERSED 0
#endif

#ifndef BLUETOOTH_MODE
#define BLUETOOTH_MODE 0
#endif

//***MOUSE STATUS***//
#ifndef NO_CLICK
#define NO_CLICK 0
#endif

#ifndef MOUSE_LEFT
#define MOUSE_LEFT 1
#endif

#ifndef MOUSE_RIGHT
#define MOUSE_LEFT 2
#endif

#ifndef MOUSE_DRAG
#define MOUSE_DRAG 3
#endif

#ifndef MOUSE_MIDDLE
#define MOUSE_MIDDLE 4
#endif


// Arduino mouse.h library defines (Documentation for easy referrel)
//#define NO_CLICK 0
//#define MOUSE_DRAG 3
// #define MOUSE_LEFT 1
// #define MOUSE_RIGHT 2
// #define MOUSE_MIDDLE 4

//***PIN ASSIGNMENTS***//

#define MODE_SELECT 12
#define PUSH_BUTTON_UP 10                          // Cursor Control Button 1: UP - digital input pin 8 (internally pulled-up)
#define PUSH_BUTTON_DOWN 9                        // Cursor Control Button 2: DOWN - digital input pin 7 (internally pulled-up)
#define LED_1 11                                   //* LipSync LED Color1 : GREEN - digital output pin 5
#define LED_2 6                                   // LipSync LED Color2 : RED - digital outputpin 4

#define PRESSURE_CURSOR A4                        //* Sip & Puff Pressure Transducer Pin - analog input pin A4
#define X_DIR_HIGH A0                             // X Direction High (Cartesian positive x : right) - analog input pin A0
#define X_DIR_LOW A1                              // X Direction Low (Cartesian negative x : left) - digital output pin A1
#define Y_DIR_HIGH A2                             // Y Direction High (Cartesian positive y : up) - analog input pin A2
#define Y_DIR_LOW A3                             //* Y Direction Low (Cartesian negative y : down) - analog input pin A3

//***VARIABLE DECLARATION***//
int xh, yh, xl, yl;                               // xh: x-high, yh: y-high, xl: x-low, yl: y-low
int x_right, x_left, y_up, y_down;                // individual neutral starting positions for each FSR

//int xh_max, xl_max, yh_max, yl_max;               // may just declare these variables but not initialize them because
// these values will be pulled from the EEPROM

float constant_radius = 30.0;                     // constant radius is initialized to 30.0 but may be changed in joystick initialization
float xh_yh_radius, xh_yl_radius, xl_yl_radius, xl_yh_radius;
float xh_yh, xh_yl, xl_yl, xl_yh;
int box_delta;                                    // the delta value for the boundary range in all 4 directions about the x,y center
int cursor_delta;                                 // amount cursor moves in some single or combined direction
int speed_counter = SPEED_COUNTER_SETTING;                            // cursor speed counter
int cursor_click_status = 0;                      // value indicator for click status, ie. tap, back and drag
int comm_mode = 0;                                // 0 == USB Communications or 1 == Bluetooth Communications
int config_done;                                  // Binary check of completed Bluetooth configuration
unsigned int puff_count, sip_count;               // int puff and long sip incremental counter :: changed from unsigned long to unsigned int

int poll_counter = 0;                             // cursor poll counter
int init_counter_A = 0;                           // serial port initialization counter
int init_counter_B = 0;                           // serial port initialization counter

// Cursor Variables
int default_cursor_speed = 6;
int delta_cursor_speed = 1;

int cursor_delay;
float cursor_factor;
int cursor_max_speed;

float yh_comp = 1.0;
float yl_comp = 1.0;
float xh_comp = 1.0;
float xl_comp = 1.0;

float yh_check, yl_check, xh_check, xl_check;
int xhm_check, xlm_check, yhm_check, ylm_check;
float sip_threshold, puff_threshold, cursor_click, cursor_back;

typedef struct {
  int _delay;
  float _factor;
  int _max_speed;
} _cursor;



_cursor setting1 = {5, -1.1, default_cursor_speed - (4 * delta_cursor_speed)}; // 5,-1.0,10
_cursor setting2 = {5, -1.1, default_cursor_speed - (3 * delta_cursor_speed)}; // 5,-1.2,10
_cursor setting3 = {5, -1.1, default_cursor_speed - (2 * delta_cursor_speed)};
_cursor setting4 = {5, -1.1, default_cursor_speed - (delta_cursor_speed)};
_cursor setting5 = {5, -1.1, default_cursor_speed};
_cursor setting6 = {5, -1.1, default_cursor_speed + (delta_cursor_speed)};
_cursor setting7 = {5, -1.1, default_cursor_speed + (2 * delta_cursor_speed)};
_cursor setting8 = {5, -1.1, default_cursor_speed + (3 * delta_cursor_speed)};
_cursor setting9 = {5, -1.1, default_cursor_speed + (4 * delta_cursor_speed)};


_cursor cursor_params[9] = {setting1, setting2, setting3, setting4, setting5, setting6, setting7, setting8, setting9};

int single = 0;
int puff1, puff2;



// Our hardcoded values --> Need to later put into EEPROM or APP Config or something
//float yh_comp = 1.0;
//float yl_comp = 1.0;
//float xh_comp = 1.0;
//float xl_comp = 1.0;
float xh_max = 756;
float xl_max = 750;
float yh_max = 798;
float yl_max = 746;

//-----------------------------------------------------------------------------------------------------------------------------------

//***MICROCONTROLLER AND PERIPHERAL MODULES CONFIGURATION***//

void setup() {

  Serial.begin(115200);                           // setting baud rate for serial coms for diagnostic data return from Bluetooth and microcontroller ***MAY REMOVE LATER***

  if( BLUETOOTH_MODE ) {
    Bluetooth_Initialization();
  }
  
  pinMode(LED_1, OUTPUT);                         // visual feedback #1
  pinMode(LED_2, OUTPUT);                         // visual feedback #2

  Mouse.begin();
  pinMode(PRESSURE_CURSOR, INPUT);                // pressure sensor pin input
  pinMode(X_DIR_HIGH, INPUT);                     // redefine the pins when all has been finalized
  pinMode(X_DIR_LOW, INPUT);                      // ditto above
  pinMode(Y_DIR_HIGH, INPUT);                     // ditto above
  pinMode(Y_DIR_LOW, INPUT);                      // ditto above

  pinMode(MODE_SELECT, INPUT);             // LOW: USB (default with jumper in) HIGH: Bluetooth (jumper removed)

  pinMode(PUSH_BUTTON_UP, INPUT_PULLUP);          // increase cursor speed button
  pinMode(PUSH_BUTTON_DOWN, INPUT_PULLUP);        // decrease cursor speed button

  delay(2000);                                    // DO NOT REMOVE DELAY!!!

  Joystick_Initialization();                      // home joystick and generate movement threshold boundaries
  delay(10);
  Pressure_Sensor_Initialization();
  delay(10);
  //Serial_Initialization();
  delay(10);
  Mouse_Configure();                              // conditionally activate the HID mouse functions
  delay(10);                                // conditionally configure the Bluetooth module [WHAT IF A NEW BT MODULE IS INSTALLED?]

  int exec_time = millis();
  Serial.print("Configuration time: ");
  Serial.println(exec_time);

  blink(4, 250, 3);                               // end initialization visual feedback

  Force_Cursor_Display();
  Display_Feature_List();

  cursor_delay = cursor_params[speed_counter]._delay;
  cursor_factor = cursor_params[speed_counter]._factor;
  cursor_max_speed = cursor_params[speed_counter]._max_speed;

  Serial.print("cursor_delay: ");
  Serial.println(cursor_delay);

  Serial.print("cursor_factor: ");
  Serial.println(cursor_factor);

  Serial.print("cursor_max_speed: ");
  Serial.println(cursor_max_speed);

  
  //cursor_max_speed = 5; // WLL - March 17 Need to delete later
  // Find Neutral Positions At Startup
  x_right = analogRead(X_DIR_HIGH);            // Initial neutral x-high value of joystick
  delay(10);

  x_left = analogRead(X_DIR_LOW);             // Initial neutral x-low value of joystick
  delay(10);

  y_up = analogRead(Y_DIR_HIGH);            // Initial neutral y-high value of joystick
  delay(10);

  y_down = analogRead(Y_DIR_LOW);             // Initial neutral y-low value of joystick

  Serial.println("=========== INIT ============");
  Serial.println(x_right);
  Serial.println(x_left);
  Serial.println(y_up);
  Serial.println(y_down);
  Serial.println("=========== INIT  DONE ============");
      
  
}

//-----------------------------------------------------------------------------------------------------------------------------------

//***START OF INFINITE LOOP***//

void loop() {

  xh = analogRead(X_DIR_HIGH);                    // A0
  xl = analogRead(X_DIR_LOW);                     // A1
  yh = analogRead(Y_DIR_HIGH);                    // A2
  yl = analogRead(Y_DIR_LOW);                     // A3

/*
  Serial.print("xh:");
  Serial.println(xh);

  Serial.print("xl:");
  Serial.println(xl);

  Serial.print("yh:");
  Serial.println(yh);

  Serial.print("yl:");
  Serial.println(yl);

  Serial.println("-----");
  */

  xh_yh = sqrt(sq(((xh - x_right) > 0) ? (float)(xh - x_right) : 0.0) + sq(((yh - y_up) > 0) ? (float)(yh - y_up) : 0.0));     // sq() function raises input to power of 2, returning the same data type int->int ...
  xh_yl = sqrt(sq(((xh - x_right) > 0) ? (float)(xh - x_right) : 0.0) + sq(((yl - y_down) > 0) ? (float)(yl - y_down) : 0.0));   // the sqrt() function raises input to power 1/2, returning a float type
  xl_yh = sqrt(sq(((xl - x_left) > 0) ? (float)(xl - x_left) : 0.0) + sq(((yh - y_up) > 0) ? (float)(yh - y_up) : 0.0));      // These are the vector magnitudes of each quadrant 1-4. Since the FSRs all register
  xl_yl = sqrt(sq(((xl - x_left) > 0) ? (float)(xl - x_left) : 0.0) + sq(((yl - y_down) > 0) ? (float)(yl - y_down) : 0.0));    // a larger digital value with a positive application force, a large negative difference

  if ((xh_yh > xh_yh_radius) || (xh_yl > xh_yl_radius) || (xl_yl > xl_yl_radius) || (xl_yh > xl_yh_radius)) {

    poll_counter++;

    //delay(20);    // originally 15 ms

    if (poll_counter >= 3) {

      // Wired Mode
      if (!BLUETOOTH_MODE) {

        if ((xh_yh >= xh_yl) && (xh_yh >= xl_yh) && (xh_yh >= xl_yl)) {
          //Serial.println("quad1");
          Serial.println(x_cursor_high(xh), y_cursor_high(yh));
          Mouse.move(x_cursor_high(xh), y_cursor_high(yh), 0);
          delay(cursor_delay);
          poll_counter = 0;
        } else if ((xh_yl > xh_yh) && (xh_yl > xl_yl) && (xh_yl > xl_yh)) {
          //Serial.println("quad4");
          Mouse.move(x_cursor_high(xh), y_cursor_low(yl), 0);
          delay(cursor_delay);
          poll_counter = 0;
        } else if ((xl_yl >= xh_yh) && (xl_yl >= xh_yl) && (xl_yl >= xl_yh)) {
          //Serial.println("quad3");
          Mouse.move(x_cursor_low(xl), y_cursor_low(yl), 0);
          delay(cursor_delay);
          poll_counter = 0;
        } else if ((xl_yh > xh_yh) && (xl_yh >= xh_yl) && (xl_yh >= xl_yl)) {
          //Serial.println("quad2");
          Mouse.move(x_cursor_low(xl), y_cursor_high(yh), 0);
          delay(cursor_delay);
          poll_counter = 0;
        }
      } else {

        if ((xh_yh >= xh_yl) && (xh_yh >= xl_yh) && (xh_yh >= xl_yl)) {
          //Serial.println("quad1");
          ble_mouseCommand(cursor_click_status, x_cursor_high(xh), y_cursor_high(yh),0 );
          delay(cursor_delay);
          poll_counter = 0;
        } else if ((xh_yl > xh_yh) && (xh_yl > xl_yl) && (xh_yl > xl_yh)) {
          //Serial.println("quad4");
          ble_mouseCommand(cursor_click_status, x_cursor_high(xh), y_cursor_low(yl),0 );
          delay(cursor_delay);
          poll_counter = 0;
        } else if ((xl_yl >= xh_yh) && (xl_yl >= xh_yl) && (xl_yl >= xl_yh)) {
          //Serial.println("quad3");
          ble_mouseCommand(cursor_click_status, x_cursor_low(xl), y_cursor_low(yl),0 );
          delay(cursor_delay);
          poll_counter = 0;
        } else if ((xl_yh > xh_yh) && (xl_yh >= xh_yl) && (xl_yh >= xl_yl)) {
          //Serial.println("quad2");
          ble_mouseCommand(cursor_click_status, x_cursor_low(xl), y_cursor_high(yh),0 );
          delay(cursor_delay);
          poll_counter = 0;
        }
      }
    }
  }

  //cursor speed control push button functions below

  if (digitalRead(PUSH_BUTTON_UP) == LOW) {
    delay(250);
    if (digitalRead(PUSH_BUTTON_DOWN) == LOW) {
      Joystick_Calibration();
    } else {
      increase_cursor_speed();      // increase cursor speed with push button up
    }
  }

  if (digitalRead(PUSH_BUTTON_DOWN) == LOW) {
    delay(250);
    if (digitalRead(PUSH_BUTTON_UP) == LOW) {
      Joystick_Calibration();
    } else {
      decrease_cursor_speed();      // decrease cursor speed with push button down
    }
  }

  //pressure sensor sip and puff functions below

  cursor_click = (((float)analogRead(PRESSURE_CURSOR)) / 1023.0) * 5.0;

  if (is_puffed(cursor_click)) {
    while (is_puffed(cursor_click)) {
      cursor_click = (((float)analogRead(PRESSURE_CURSOR)) / 1023.0) * 5.0;
      puff_count++;         // NEED TO FIGURE OUT ROUGHLY HOW LONG ONE CYCLE OF THIS WHILE LOOP IS -> COUNT THRESHOLD
      delay(5);
    }
    Serial.println("Puffed: ");
    Serial.println(puff_count);             //***REMOVE

    if (!BLUETOOTH_MODE) {
      if (puff_count < SIPPUFF_SECONDARY_DURATION_THRESHOLD_SETTING * 150) {
        if (Mouse.isPressed(MOUSE_LEFT)) {
          Mouse.release(MOUSE_LEFT);
        } else {
          Mouse.click(MOUSE_LEFT);
          delay(5);
        }
      } else if (puff_count > SIPPUFF_SECONDARY_DURATION_THRESHOLD_SETTING * 150 && puff_count < SIPPUFF_TERTIARY_DURATION_THRESHOLD_SETTING * 150) {
        Serial.println("Is this supposed to drag?");
        if (Mouse.isPressed(MOUSE_LEFT)) {
          Mouse.release(MOUSE_LEFT);
        } else {
          Mouse.press(MOUSE_LEFT);
          delay(5);
        }
      } else if (puff_count > SIPPUFF_TERTIARY_DURATION_THRESHOLD_SETTING * 150) {
        blink(4, 350, 3);   // visual prompt for user to release joystick for automatic calibration of home position
        Manual_Joystick_Home_Calibration();
      }
    } else {
      if (puff_count < SIPPUFF_SECONDARY_DURATION_THRESHOLD_SETTING * 150) {
        cursor_click_status = 1; 
        ble_mouseCommand(cursor_click_status, 0, 0, 0);
        cursor_click_status = 0;
        delay(5);
      } else if (puff_count > SIPPUFF_SECONDARY_DURATION_THRESHOLD_SETTING * 150 && puff_count < SIPPUFF_TERTIARY_DURATION_THRESHOLD_SETTING * 150) {
        if (cursor_click_status == 0) {
          cursor_click_status = 3;
          ble_mouseCommand(cursor_click_status, 0, 0, 0);
          cursor_click_status = 0;
          delay(5);
        } 
        /*else if (cursor_click_status == 3) {
          cursor_click_status = 0;
          ble_mouseCommand(cursor_click_status, 0, 0, 0);
          cursor_click_status = 3;
        } */

      } else if (puff_count > SIPPUFF_TERTIARY_DURATION_THRESHOLD_SETTING * 150) {
        blink(4, 350, 3);     // visual prompt for user to release joystick for automatic calibration of home position
        Manual_Joystick_Home_Calibration();
      }
    }

    puff_count = 0;
  }

  if (is_sipped(cursor_click)) {
    while (is_sipped(cursor_click)) {
      cursor_click = (((float)analogRead(PRESSURE_CURSOR)) / 1023.0) * 5.0;
      sip_count++;         // NEED TO FIGURE OUT ROUGHLY HOW LONG ONE CYCLE OF THIS WHILE LOOP IS -> COUNT THRESHOLD
      delay(5);
    }
    Serial.print("Sipped: ");
    Serial.println(sip_count);             //***REMOVE

    if (!BLUETOOTH_MODE) {
      if (sip_count < SIPPUFF_SECONDARY_DURATION_THRESHOLD_SETTING * 150) {
        Mouse.click(MOUSE_RIGHT);
        delay(5);
      } else if (sip_count > SIPPUFF_SECONDARY_DURATION_THRESHOLD_SETTING * 150 && sip_count < SIPPUFF_TERTIARY_DURATION_THRESHOLD_SETTING * 150) {
        mouseScroll();
        delay(5);
      } else if (sip_count > SIPPUFF_TERTIARY_DURATION_THRESHOLD_SETTING * 150) {
        sip_secondary();
        delay(5);
      }
    } else {
      if (sip_count < SIPPUFF_SECONDARY_DURATION_THRESHOLD_SETTING * 150) {
        cursor_click_status = 2;
        //mouseCommand(cursor_click_status, 0, 0, 0);
        ble_mouseCommand(cursor_click_status, 0, 0, 0);
        //cursor_click_status = 0;
        //mouseClear();
        delay(5);
      } else if (sip_count > SIPPUFF_SECONDARY_DURATION_THRESHOLD_SETTING * 150 && sip_count < SIPPUFF_TERTIARY_DURATION_THRESHOLD_SETTING * 150) {
        mouseScroll();
        delay(5);
      } else if (sip_count > SIPPUFF_TERTIARY_DURATION_THRESHOLD_SETTING * 150) {
        sip_secondary();
        delay(5);
      }
    }
    sip_count = 0;
  }
}

//***END OF INFINITE LOOP***//

//-----------------------------------------------------------------------------------------------------------------------------------


void Display_Feature_List(void) {

  Serial.println(" ");
  Serial.println(" --- ");
  Serial.println("This is the 09 May - FSR Bluetooth Euclidean WIP");
  Serial.println(" ");
  Serial.println("Enhanced functions:");
  Serial.println(" ");
  Serial.println("Tap and drag");
  Serial.println("Scrolling");
  Serial.println("Joystick calibration");
  Serial.println("Middle mouse button");
  Serial.println("Hands-free home positioning reset");
  Serial.println("Cursor security swipe function");
  Serial.println("Bluetooth capable connectivity");
  Serial.println(" --- ");
  Serial.println(" ");

}

//***CURSOR: SIP SECONDARY FUNCTION SELECTION***//
void sip_secondary(void) {
  while (1) {

    xh = analogRead(X_DIR_HIGH);                    // A0 :: NOT CORRECT MAPPINGS
    xl = analogRead(X_DIR_LOW);                     // A1
    yh = analogRead(Y_DIR_HIGH);                    // A2
    yl = analogRead(Y_DIR_LOW);                     // A10

    digitalWrite(LED_2, HIGH);

    if (xh > (x_right + 50)) {
      mouse_middle_button();
      break;
    } else if (xl > (x_left + 50)) {
      mouse_middle_button();
      break;
    } else if (yh > (y_up + 50)) {
      cursor_swipe();
      break;
    } else if (yl > (y_down + 50)) {
      cursor_swipe();
      break;
    }
  }
  digitalWrite(LED_2, LOW);
}

void cursor_swipe(void) {
  Serial.println("gotcha");
  if (!BLUETOOTH_MODE) {

    for (int i = 0; i < 3; i++) Mouse.move(0, 126, 0);
    Mouse.press(MOUSE_LEFT);
    delay(125);

    for (int j = 0; j < 3; j++) Mouse.move(0, -126, 0);
    Mouse.release(MOUSE_LEFT);
    delay(125);
  } else {

    cursor_click_status = 0;
    for (int i = 0; i < 3; i++) 
    ble_mouseCommand(cursor_click_status, 0, 126, 0);
    delay(125);

    cursor_click_status = 1;
    for (int j = 0; j < 3; j++) 
    ble_mouseCommand(cursor_click_status, 0, -126, 0);
    cursor_click_status = 0;
    delay(125);
  }
}

void mouse_middle_button(void) {
  if (!BLUETOOTH_MODE) {
    Mouse.click(MOUSE_MIDDLE);
    delay(125);
  } else {
    //cursor_click_status = 0x05;
    Serial.println("MOUSE MIDDLE BLUETOOTH");
    cursor_click_status = 4;
    ble_mouseCommand(cursor_click_status, 0, 0, 0);
    cursor_click_status = 0;
    delay(125);
  }
}

//***LED BLINK FUNCTIONS***//

void blink(int num_Blinks, int delay_Blinks, int LED_number ) {
  if (num_Blinks < 0) num_Blinks *= -1;

  switch (LED_number) {
    case 1: {
        for (int i = 0; i < num_Blinks; i++) {
          digitalWrite(LED_1, HIGH);
          delay(delay_Blinks);
          digitalWrite(LED_1, LOW);
          delay(delay_Blinks);
        }
        break;
      }
    case 2: {
        for (int i = 0; i < num_Blinks; i++) {
          digitalWrite(LED_2, HIGH);
          delay(delay_Blinks);
          digitalWrite(LED_2, LOW);
          delay(delay_Blinks);
        }
        break;
      }
    case 3: {
        for (int i = 0; i < num_Blinks; i++) {
          digitalWrite(LED_1, HIGH);
          delay(delay_Blinks);
          digitalWrite(LED_1, LOW);
          delay(delay_Blinks);
          digitalWrite(LED_2, HIGH);
          delay(delay_Blinks);
          digitalWrite(LED_2, LOW);
          delay(delay_Blinks);
        }
        break;
      }
  }
}

//***HID MOUSE CURSOR SPEED FUNCTIONS***//

void increase_cursor_speed(void) {
  speed_counter++;

  if (speed_counter == 9) {
    blink(6, 50, 3);     // twelve very fast blinks
    speed_counter = 8;
  } else {
    blink(speed_counter, 100, 1);

    cursor_delay = cursor_params[speed_counter]._delay;
    cursor_factor = cursor_params[speed_counter]._factor;
    cursor_max_speed = cursor_params[speed_counter]._max_speed;

    EEPROM.put(2, speed_counter);
    delay(25);
    Serial.println("+");
  }
}

void decrease_cursor_speed(void) {
  speed_counter--;

  if (speed_counter == -1) {
    blink(6, 50, 3);     // twelve very fast blinks
    speed_counter = 0;
  } else if (speed_counter == 0) {
    blink(1, 350, 1);

    cursor_delay = cursor_params[speed_counter]._delay;
    cursor_factor = cursor_params[speed_counter]._factor;
    cursor_max_speed = cursor_params[speed_counter]._max_speed;

    EEPROM.put(2, speed_counter);
    delay(25);
    Serial.println("-");
  } else {
    blink(speed_counter, 100, 1);

    cursor_delay = cursor_params[speed_counter]._delay;
    cursor_factor = cursor_params[speed_counter]._factor;
    cursor_max_speed = cursor_params[speed_counter]._max_speed;

    EEPROM.put(2, speed_counter);
    delay(25);
    Serial.println("-");
  }
}

//***HID MOUSE CURSOR MOVEMENT FUNCTIONS***//

int y_cursor_high(int j) {

  if (j > y_up) {
    float y_up_factor = 1.25 * (yh_comp * (((float)(j - y_up)) / (yh_max - y_up)));
    int k = (int)(round(-1.0 * pow(cursor_max_speed, y_up_factor)) - 1.0);   
    if (k <= (-1 * cursor_max_speed) ) {
      k = -1 * cursor_max_speed;
      return k;
    } else if ( (k < 0) && (k > (-1 * cursor_max_speed))) {
      return k;
    } else {
      k = 0;
      return k;
    }
  } else {
    return 0;
  }
}

int y_cursor_low(int j) {

  if (j > y_down) {

    float y_down_factor = 1.25 * (yl_comp * (((float)(j - y_down)) / (yl_max - y_down)));
    
    int k = (int)(round(1.0 * pow(cursor_max_speed, y_down_factor)) - 1.0);
    
    if (k >= cursor_max_speed) {

      k = cursor_max_speed;
      return k;
    } else if ((k > 0) && (k < cursor_max_speed)) {
      return k;
    } else {
      k = 0;
      return k;
    }
  } else {
    return 0;
  }
}

int x_cursor_high(int j) {

  if (j > x_right) {

    float x_right_factor = 1.25 * (xh_comp * (((float)(j - x_right)) / (xh_max - x_right)));
    
    int k = (int)(round(1.0 * pow(cursor_max_speed, x_right_factor)) - 1.0);

    if (k >= cursor_max_speed) {

      k = cursor_max_speed;
      return k;
    } else if ((k > 0) && (k < cursor_max_speed)) {
      return k;
    } else {
      k = 0;
      return k;
    }
  } else {
    return 0;
  }
}

int x_cursor_low(int j) {

  if (j > x_left) {

    float x_left_factor = 1.25 * (xl_comp * (((float)(j - x_left)) / (xl_max - x_left)));

    int k = (int)(round(-1.0 * pow(cursor_max_speed, x_left_factor)) - 1.0);

    if ( k <= (-1 * cursor_max_speed) ) {
      k = -1 * cursor_max_speed;
      return k;
    } else if ( (k < 0) && (k > -1 * cursor_max_speed)) {
      return k;
    } else {
      k = 0;
      return k;
    }
  } else {
    return 0;
  }
}

//***BLUETOOTH HID MOUSE FUNCTIONS***//

void ble_mouseCommand(int buttons, int x, int y, int scroll ) {

  // Move mouse only (no click)
    ble.print(F("AT+BleHidMouseMove="));
    ble.print(x);
    ble.print(",");
    ble.print(y);
    ble.print(",");
    ble.print(scroll);
    ble.println(",");
    
    // Clicking
    if( buttons == MOUSE_LEFT ) { // Left Click
      ble.println(F("AT+BLEHIDMOUSEBUTTON=L,doubleclick"));
    }

    if( buttons == MOUSE_RIGHT ) { // Right Click
       ble.println(F("AT+BLEHIDMOUSEBUTTON=R"));
    }

    if( buttons == MOUSE_DRAG ) {
      ble.println(F("AT+BLEHIDMOUSEBUTTON=L"));
    }
    
    if( buttons == MOUSE_MIDDLE ) { // Middle Button
      ble.println(F("AT+BLEHIDMOUSEBUTTON=M"));
      Serial.println("MIDDLE BUTTON BT MODE");
    }
    
}

void mouseCommand(int buttons, int x, int y, int scroll) {

  byte BTcursor[7];

  BTcursor[0] = 0xFD;
  BTcursor[1] = 0x5;
  BTcursor[2] = 0x2;
  BTcursor[3] = lowByte(buttons); //check this out
  BTcursor[4] = lowByte(x);       //check this out
  BTcursor[5] = lowByte(y);       //check this out
  BTcursor[6] = lowByte(scroll);  //check this out

  Serial1.write(BTcursor, 7);
  Serial1.flush();

  delay(10);    // reduced poll_counter delay by 30ms so increase delay(10) to delay (40)
}

/*
void mouseClear(void) {

  byte BTcursor[7];

  BTcursor[0] = 0xFD;
  BTcursor[1] = 0x5;
  BTcursor[2] = 0x2;
  BTcursor[3] = 0x00;
  BTcursor[4] = 0x00;
  BTcursor[5] = 0x00;
  BTcursor[6] = 0x00;

  Serial1.write(BTcursor, 7);
  Serial1.flush();
  delay(10);
}*/

//***MOUSE SCROLLING FUNCTION***//

void mouseScroll(void) {
  while (1) {

    int scroll_up = analogRead(Y_DIR_HIGH);                      // A2
    int scroll_down = analogRead(Y_DIR_LOW);                     // A10

    float scroll_release = (((float)analogRead(PRESSURE_CURSOR)) / 1023.0) * 5.0;

    if (!BLUETOOTH_MODE) {

      if (scroll_up > y_up + 30) {
        Mouse.move(0, 0, -1 * y_cursor_high(scroll_up));
        delay(cursor_delay * 35);   // started with this factor change as necessary
      } else if (scroll_down > y_down + 30) {
        Mouse.move(0, 0, -1 * y_cursor_low(scroll_down));
        delay(cursor_delay * 35);   // started with this factor change as necessary
      } else if ((scroll_release > sip_threshold) || (scroll_release < puff_threshold)) {
        break;
      }
    } else {

      if (scroll_up > y_up + 30) {
        ble_mouseCommand(0, 0, 0, -1 * y_cursor_high(scroll_up));
        delay(cursor_delay * 35);
      } else if (scroll_down > y_down + 30) {
        ble_mouseCommand(0, 0, 0, -1 * y_cursor_low(scroll_down));
        delay(cursor_delay * 35);
      } else if ((scroll_release > sip_threshold) || (scroll_release < puff_threshold)) {
        break;
      }

    }
  }
  delay(250);
}

//***FORCE DISPLAY OF CURSOR***//

void Force_Cursor_Display(void) {
  if (!BLUETOOTH_MODE) {
    Mouse.move(1, 0, 0);
    delay(25);
    Mouse.move(-1, 0, 0);
    delay(25);
  } else {


    /*
      mouseCommand(0, 1, 0, 0);
      delay(25);
      mouseCommand(0, -1, 0, 0);
      delay(25);
    */
  }
}


//***JOYSTICK INITIALIZATION FUNCTION***//

void Joystick_Initialization(void) {
  xh = analogRead(X_DIR_HIGH);            // Initial neutral x-high value of joystick
  delay(10);

  xl = analogRead(X_DIR_LOW);             // Initial neutral x-low value of joystick
  delay(10);

  yh = analogRead(Y_DIR_HIGH);            // Initial neutral y-high value of joystick
  delay(10);

  yl = analogRead(Y_DIR_LOW);             // Initial neutral y-low value of joystick
  delay(10);

  x_right = xh;
  x_left = xl;
  y_up = yh;
  y_down = yl;
/*
  EEPROM.get(6, yh_comp);
  delay(10);
  EEPROM.get(10, yl_comp);
  delay(10);
  EEPROM.get(14, xh_comp);
  delay(10);
  EEPROM.get(18, xl_comp);
  delay(10);
  EEPROM.get(22, xh_max);
  delay(10);
  EEPROM.get(24, xl_max);
  delay(10);
  EEPROM.get(26, yh_max);
  delay(10);
  EEPROM.get(28, yl_max);
  delay(10);
*/
  constant_radius = 30.0;                       //40.0 works well for a constant radius

  xh_yh_radius = constant_radius;
  xh_yl_radius = constant_radius;
  xl_yl_radius = constant_radius;
  xl_yh_radius = constant_radius;

}

//***PRESSURE SENSOR INITIALIZATION FUNCTION***//

void Pressure_Sensor_Initialization(void) {
  float nominal_cursor_value = (((float)analogRead(PRESSURE_CURSOR)) / 1024.0) * 5.0; // Initial neutral pressure transducer analog value [0.0V - 5.0V]

  sip_threshold = nominal_cursor_value + SIP_THRESHOLD_SETTING;    //Create sip pressure threshold value ***Larger values tend to minimize frequency of inadvertent activation

  puff_threshold = nominal_cursor_value - PUFF_THRESHOLD_SETTING;   //Create puff pressure threshold value ***Larger values tend to minimize frequency of inadvertent activation
}

//***ARDUINO/GENUINO HID MOUSE INITIALIZATION FUNCTION***//

void Mouse_Configure(void) {
  if (!BLUETOOTH_MODE) {                       // USB mode is comm_mode == 0, this is when the jumper on J13 is installed
    Mouse.begin();                            // Initialize the HID mouse functions from Mouse.h header file
    delay(25);                                // Allow extra time for initialization to take effect ***May be removed later
  }
}

//----------------------RN-42 BLUETOOTH MODULE INITIALIZATION SECTION----------------------//

//***BLUETOOTH CONFIGURATION STATUS FUNCTION***//

void BT_Config_Status(void) {
  int BT_EEPROM = 3;                               // Local integer variable initialized and defined for use with EEPROM GET function
  EEPROM.get(0, BT_EEPROM);                        // Assign value of EEPROM memory at index zero (0) to int variable BT_EEPROM
  delay(10);
  Serial.println(BT_EEPROM);                       // Only for diagnostics, may be removed later
  config_done = BT_EEPROM;                         // Assign value of local variable BT_EEPROM to global variable config_done
  delay(10);
}


//***JOYSTICK SPEED CALIBRATION***//

void Joystick_Calibration(void) {

  Serial.println("Prepare for joystick calibration!");
  Serial.println(" ");
  blink(4, 300, 3);

  Serial.println("Move mouthpiece to the furthest vertical up position and hold it there until the LED turns SOLID RED, then release the mouthpiece.");
  blink(6, 500, 1);
  yh_max = analogRead(Y_DIR_HIGH);
  blink(1, 1000, 2);
  Serial.println(yh_max);

  Serial.println("Move mouthpiece to the furthest horizontal right position and hold it there until the LED turns SOLID RED, then release the mouthpiece.");
  blink(6, 500, 1);
  xh_max = analogRead(X_DIR_HIGH);
  blink(1, 1000, 2);
  Serial.println(xh_max);

  Serial.println("Move mouthpiece to the furthest vertical down position and hold it there until the LED turns SOLID RED, then release the mouthpiece.");
  blink(6, 500, 1);
  yl_max = analogRead(Y_DIR_LOW);
  blink(1, 1000, 2);
  Serial.println(yl_max);

  Serial.println("Move mouthpiece to the furthest horizontal left position and hold it there until the LED turns SOLID RED, then release the mouthpiece.");
  blink(6, 500, 1);
  xl_max = analogRead(X_DIR_LOW);
  blink(1, 1000, 2);
  Serial.println(xl_max);

  int max1 = (xh_max > xl_max) ? xh_max : xl_max;
  int max2 = (yh_max > yl_max) ? yh_max : yl_max;
  float max_final = (max1 > max2) ? (float)max1 : (float)max2;

  //int delta_max_total = (yh_max - y_up) + (yl_max - y_down) + (xh_max - x_right) + (xl_max - x_left);

  Serial.print("max_final: ");
  Serial.println(max_final);

  //float avg_delta_max = ((float)(delta_max_total)) / 4;

  //Serial.print("avg_delta_max: ");
  //Serial.println(avg_delta_max);

  yh_comp = (max_final - y_up) / (yh_max - y_up);
  yl_comp = (max_final - y_down) / (yl_max - y_down);
  xh_comp = (max_final - x_right) / (xh_max - x_right);
  xl_comp = (max_final - x_left) / (xl_max - x_left);

  EEPROM.put(6, yh_comp);
  delay(10);
  EEPROM.put(10, yl_comp);
  delay(10);
  EEPROM.put(14, xh_comp);
  delay(10);
  EEPROM.put(18, xl_comp);
  delay(10);
  EEPROM.put(22, xh_max);
  delay(10);
  EEPROM.put(24, xl_max);
  delay(10);
  EEPROM.put(26, yh_max);
  delay(10);
  EEPROM.put(28, yl_max);
  delay(10);

  blink(5, 250, 3);

  Serial.println(" ");
  Serial.println("Joystick speed calibration procedure is complete.");
}

//***MANUAL JOYSTICK POSITION CALIBRATION***///
void Manual_Joystick_Home_Calibration(void) {

  xh = analogRead(X_DIR_HIGH);            // Initial neutral x-high value of joystick
  delay(10);
  Serial.println(xh);                     // Recommend keeping in for diagnostic purposes

  xl = analogRead(X_DIR_LOW);             // Initial neutral x-low value of joystick
  delay(10);
  Serial.println(xl);                     // Recommend keeping in for diagnostic purposes

  yh = analogRead(Y_DIR_HIGH);            // Initial neutral y-high value of joystick
  delay(10);
  Serial.println(yh);                     // Recommend keeping in for diagnostic purposes

  yl = analogRead(Y_DIR_LOW);             // Initial neutral y-low value of joystick
  delay(10);
  Serial.println(yl);                     // Recommend keeping in for diagnostic purposes

  x_right = xh;
  x_left = xl;
  y_up = yh;
  y_down = yl;

  int max1 = (xh_max > xl_max) ? xh_max : xl_max;
  int max2 = (yh_max > yl_max) ? yh_max : yl_max;
  float max_final = (max1 > max2) ? (float)max1 : (float)max2;

  //int delta_max_total = (yh_max - y_up) + (yl_max - y_down) + (xh_max - x_right) + (xl_max - x_left);

  Serial.print("max_final: ");
  Serial.println(max_final);

  //float avg_delta_max = ((float)(delta_max_total)) / 4; // sdfjefakdfjlaskd

  //Serial.print("avg_delta_max: ");
  //Serial.println(avg_delta_max);

  yh_comp = (max_final - y_up) / (yh_max - y_up);
  yl_comp = (max_final - y_down) / (yl_max - y_down);
  xh_comp = (max_final - x_right) / (xh_max - x_right);
  xl_comp = (max_final - x_left) / (xl_max - x_left);

  EEPROM.put(6, yh_comp);
  delay(10);
  EEPROM.put(10, yl_comp);
  delay(10);
  EEPROM.put(14, xh_comp);
  delay(10);
  EEPROM.put(18, xl_comp);
  delay(10);

  Serial.println("Home position calibration complete.");
}

//***SPECIAL INITIALIZATION OPERATIONS***//

void Serial_Initialization(void) {
  while (!Serial1) {
    while (!Serial) {
      if (init_counter_A < 100) {
        delay(5);
        init_counter_A++;
      } else {
        break;
      }
    }
    if (init_counter_B < 100) {
      delay(5);
      init_counter_B++;
    } else {
      break;
    }
  }
  delay(10);
  Serial.println(init_counter_A);
  Serial.println(init_counter_B);
  Serial.println("Serial and Serial1 are good!");
}

void Bluetooth_Initialization(void) {
  Serial.println("Bluetooth Mode");
  // Initialise the BLE module
  if ( !ble.begin(VERBOSE_MODE) ){
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  } 
  /* Enable HID Service (including sensor) */
  Serial.println(F("Enable HID Service (including sensor): "));
  if (! ble.sendCommandCheckOK(F( "AT+BleHIDEn=On"  ))) {
    error(F("Failed to enable HID (firmware >=0.6.6?)"));
  }
  
  Serial.println(F("Setting device name to 'LipSync': "));
  if (! ble.sendCommandCheckOK(F( "AT+GAPDEVNAME=LipSync" )) ) {
    error(F("Could not set device name?"));
  }
}

// Following functions have been added to enable the reversal of sipping and puffing

/* Returns True if user sips and sets click as sip
 *         True if user puffs and sets click as puff
 *         False Otherwise
 *         is_leftcliked
 */
int is_sipped(double cursor_click) {
  if (SIP_PUFF_REVERSED) {
    return cursor_click < puff_threshold;
  }

  return cursor_click > sip_threshold;
}

/* Retruns True if user sips and sets right click as sip 
*           True if user puffs and sets right click as puff
*         False Otherwise
*         is_rightclicked
*/
int is_puffed(double cursor_click) {
  if (SIP_PUFF_REVERSED) {
    return cursor_click > sip_threshold;
  }

  return cursor_click < puff_threshold;
}
