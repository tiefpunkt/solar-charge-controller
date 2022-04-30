
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <PID_v2.h>

// OLED
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// PINS
#define CHARGE_PWM 6
#define LOAD 9
#define LED_RED 7
#define LED_GREEN 8

// CHARGING PARAMETERS
#define OVERSAMPLING 8
#define MIN_VOLTAGE 5.4*2
#define CC_CV_LIMIT 6.96*2
#define MAX_VOLTAGE 7.2*2

// Specify the links and initial tuning parameters
double Kp = 2, Ki = 5, Kd = 1;
PID_v2 myPID(Kp, Ki, Kd, PID::Direct);

float solar_volt = 0; // variable for solar panel voltage
float bat_volt = 0;  // variable for battery voltage
float sample1 = 0;   // reading form Arduino pin A0
float sample2 = 0;   // reading form Arduino pin A1

int charged_percent = 0;
double dutycycle = 0;
int backLight = 13;  // pin 13 will control the backlight

void setup()
{
  TCCR0B = TCCR0B & 0b11111000 | 0x05; // setting prescaar for 61.03Hz pwm
  Serial.begin(9600);
  pinMode(CHARGE_PWM, OUTPUT);
  pinMode(LOAD, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(CHARGE_PWM, LOW);
  digitalWrite(LOAD, LOW);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
  pinMode(backLight, OUTPUT);          //set pin 13 as output
  analogWrite(backLight, 150);        //controls the backlight intensity 0-255

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);
  analogReference(INTERNAL);

  myPID.Start(analogRead(A1),  // input
              0,                      // current output
              MAX_VOLTAGE);                   // setpoint
}
void loop()
{
  //lcd.setCursor(16,1); // set the cursor outside the display count
  //lcd.print(" ");      // print empty character

  ///////////////////////////  VOLTAGE SENSING ////////////////////////////////////////////
  sample1 = 0;
  sample2 = 0;
  for (int i = 0; i < OVERSAMPLING; i++)
  {
    sample1 += analogRead(A0); //read the input voltage from solar panel
    sample2 += analogRead(A1); //read the battery voltage
    delay(2);
  }
  sample1 = sample1 / OVERSAMPLING;
  sample2 = sample2 / OVERSAMPLING;
  // actual volt/divider output=3.127
  //2.43 is eqv to 520 ADC
  // 1 is eqv to .004673
  //solar_volt=(sample1*4.673* 3.127)/1000;
  //bat_volt=(sample2*4.673* 3.127)/1000;
  solar_volt = sample1 * 30.8 / 1024;
  bat_volt = sample2 * 30.8 / 1024;
  //Serial.print("solar input voltage :");
  //Serial.println(solar_volt);
  //Serial.print("battery voltage :");
  //Serial.println(bat_volt);

  // ///////////////////////////PWM BASED CHARGING ////////////////////////////////////////////////
  // As battery is gradually charged the charge rate (pwm duty) is decreased
  // 7.2v = fully charged(100%)
  //6v =fully discharged(0%)
  // when battery voltage is less than 6.2v, give you alart by glowing RED LED and displaying "DISCHARGED..."

  if ((solar_volt > bat_volt) && ( bat_volt <= CC_CV_LIMIT ))
  {
    dutycycle = 255;
    //analogWrite(CHARGE_PWM, 242.25); // @ 95% duty // boost charging// most of the charging done here
    //analogWrite(CHARGE_PWM, 254);
    //Serial.print("pwm duty cycle is :");
    //Serial.println("95%");
  }
  else if ((solar_volt > bat_volt) && (bat_volt > CC_CV_LIMIT))
  {
    //dutycycle = 26;
    //dutycycle = 242 * MAX_VOLTAGE / solar_volt;
    if (bat_volt < MAX_VOLTAGE && dutycycle <= 255 ) {
      dutycycle++;
    } else if (bat_volt > MAX_VOLTAGE && dutycycle > 0) {
      dutycycle--;
    }
    // dutycycle = (242 * MAX_VOLTAGE / solar_volt) + myPID.Run(bat_volt);
    //analogWrite(CHARGE_PWM, 25.5); // 10% duty // float charging
    //Serial.print("pwm duty cycle is :");
    //Serial.println("10%");
  }
  // // shut down when battery is fully charged or when sunlight is not enough
  else if  (solar_volt < bat_volt) {
    dutycycle = 0;
    //analogWrite(CHARGE_PWM, 0);
    //Serial.print("pwm duty cycle is :");
    //Serial.println("0%");
    //digitalWrite(LED_GREEN, LOW); // green LED will off as no charging is done during this time
  }
  analogWrite(CHARGE_PWM, dutycycle);
  ///////////////////////////////////////// BATTERY STATUS INDICATOR  ////////////////////////////////////////////////
  //The map() function uses integer math so will not generate fractions
  // so I multiply battery voltage with 10 to convert float into a intiger value
  // when battery voltage is 6.0volt it is totally discharged ( 6*10 =60)
  // when battery voltage is 7.2volt it is fully charged (7.2*10=72)
  // 6.0v =0% and 7.2v =100%
  charged_percent = map(bat_volt * 10, (int)(MIN_VOLTAGE * 10) , (int)(MAX_VOLTAGE * 10), 0, 100);
  /*
    if (solar_volt > bat_volt)&&( bat_volt <=7.2))
    {
    Serial.print (charged_percent);
    Serial.println("% charged");
    Serial.println("");
    Serial.println("**********************************************************************************");
    }

    else if (bat_volt < 6)
    {
    Serial.println("BATTERY IS DEAD !!!!! ");
    }
  */
  //////////////////////////////////// SERIAL OUTPUT /////////
  Serial.print("voltage_solar:");
  Serial.print(solar_volt);
  Serial.print(",voltage_battery:");
  Serial.print(bat_volt);
  Serial.print(",dutycycle:");
  Serial.print(map(dutycycle, 0, 254, 0, 100));
  Serial.print(",battery_charge:");
  Serial.println(charged_percent);

  ////////////////////////////////////LCD DISPLAY /////////////////////////////////////////////////////
  display.clearDisplay();
  display.setCursor(0, 0); // set the cursor at 1st col and 1st row
  display.print("SOL:");
  display.print(solar_volt);
  display.print(" BAT:");
  display.print(bat_volt);
  display.setCursor(1, 1 * 8); // set the cursor at 1st col and 2nd row
  // LCD will show the %charged during charging period only
  if ((bat_volt > MIN_VOLTAGE) && (bat_volt <= MAX_VOLTAGE))
  {
    display.print(charged_percent);
    display.print("% Charged ");
  }
  // LCD will alart when battery is dead by displaying the message "BATTERY IS DEAD!!"
  else if (bat_volt < MIN_VOLTAGE)
  {
    display.print("BATTERY IS DEAD!!");
  }

  display.display();
  /////////////////////////////LOAD ////LED INDICATION ///////////////////////////////////////////////////////
  if ((solar_volt < 3 ) && (bat_volt > 6.2))
    // when there is no sunlight(night) and battery is charged,
    //load will switched  on automatically
  {
    digitalWrite(LOAD, HIGH);
  }
  ///load will be disconnected during day time(solar_volt > 6) or when battery is discharged condition
  if ((bat_volt < 6.2 ) or (solar_volt > 6 ))
  {
    digitalWrite(LOAD, LOW); // prevent battery from complete discharging

  }
  //////////////////////////////LED INDICATION DURING CHARGING////////////////////////////////////////

  if (  solar_volt > bat_volt && bat_volt < MAX_VOLTAGE)
  {
    ///Green LED will blink continiously indicating charging is going on
    digitalWrite(LED_GREEN, HIGH);
    delay(5);
    digitalWrite(LED_GREEN, LOW);
    delay(5);
  }
  // Red LED will glow when battery is discharged
  // also display in LCD
  if (bat_volt < 6.2 && bat_volt > 6)
    // seecond restriction is given for indicating battery is dead
    // if you omit the (bat_volt > 6) when ever battery is dead also display bat discharged
  {
    digitalWrite(LED_RED, HIGH); // indicating battery is discharged
    display.setCursor(1, 1);
    display.print("BAT DISCHARGED..");
  }
  // Red LED will OFF when battery is not discharged
  if (bat_volt > 6.2)
  {
    digitalWrite(LED_RED, LOW);
  }
  //Green LED will glow when battery is fully charged
  if (bat_volt >= MAX_VOLTAGE)
  {
    digitalWrite(LED_GREEN, HIGH);
  }
  //delay(50);
}
