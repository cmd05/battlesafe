//code:

//With some Flags

#include "MAX30100_PulseOximeter.h"
#include <LiquidCrystal.h>
#include <Wire.h>

const String GPS_POSITION_STR = "56J  MS  80443  25375";

LiquidCrystal lcd(7, 8, 9, 10, 11, 12);   // put your Arduino pin numbers here

const unsigned long pox_sampling_time = 500;  // in milliseconds
const unsigned long max_pox_wait      = 2000; // ms

PulseOximeter pox; //we will be calling Pulseoximeter as pox

const int pox_nr_readings = 15; // number of reading we want to average out
float weighted_avg = 0.4; // give weights to current value and stored value while averaging
uint32_t last_report_generated = 0; //store the time when last report was displyed on the screen
uint32_t last_beat_detected = 0; //store the time when last beat was detected
int pox_readings = 0; // store the number of readings collected and compare it with number of readings for averaging
int average_heart_rate = 0; // initialization of the parameters
int average_SPO2 = 0; // initialization of the parameters
byte Heart_rate = 0; // initialization of the parameters
byte SPO2 = 0; // initialization of the parameters
bool pox_initialized = false; //flag that tells the code has been initialized
bool pox_calculating_values = false; //flag that tells values are being calculated.

int last_heart_rate = 0;
int last_SPO2 = 0;


int ADXL345 = 0x53; // The ADXL345 sensor I2C address
float ax_out, ay_out, az_out;  // Outputs

constexpr int max_free_fall_acc = 0.35;
constexpr int free_fall_debounce_time = 1000;
int last_fall_time = 0;


const int BUTTON_PIN = 5; // the number of the pushbutton pin
const int LONG_PRESS_TIME  = 1500; // 1000 milliseconds
const int BUTTON_CONFIRMATION_TIME = 5000;

// Variables will change:
int lastState = LOW;  // the previous state from the input pin
int currentState;     // the current reading from the input pin
unsigned long pressedTime  = 0;
bool isPressing = false;
bool isLongDetected = false;

void emergency(String message, bool confirm = true);

//Defining a function for initializing the display at the start
void displayinitially(){

  if (!pox_initialized){ // if system is not initialized then display the message and initiate it

    Serial.println("");

    Serial.println("USER: Please place your finger on the sensor");
    lcd.clear();

    pox_initialized = true; // as we are initializing here

  }

}



// Defining function of beat detection. On detection of a beat store the time of beat in last_beat_detected variable

void beatdetected(){

  last_beat_detected = millis();
  // Serial.println("BEAT!");
}



// Next define a function that displays something on screen when user is waiting for the calculated Heart rate and SpO2.

int N_DOTS = 0;

void displayduringcalc(int i){ // int i will make the display continuous 

  if (not pox_calculating_values){ // if calculating values is false

    pox_calculating_values = true; // make it true as here we will be calculating values 

    pox_initialized = false; // remove initialization message of "Please place your finger on the sensor"

    Serial.println("USER: Processing pulse values...");
  }

  Serial.print(" .");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Reading");

  lcd.setCursor(0, 1);
  if(N_DOTS == 0) {
    lcd.print(".");
    N_DOTS++;
  } else if(N_DOTS == 1) {
    lcd.print("..");
    N_DOTS++;
  } else if(N_DOTS == 2) {
    lcd.print("...");
    N_DOTS = 0;
  }
}



//Defining function that will display the calculated Heart rate and SpO2 values

void output_display(){
  // if outside clamped regions:
  if(average_heart_rate > 105)
    average_heart_rate = 105;
  else if(average_heart_rate < 63)
    average_heart_rate = 63;

  if(average_SPO2 > 99)
    average_SPO2 = 99;
  if(average_SPO2 < 90)
    average_SPO2 = 90;


  Serial.println("");

  Serial.println("PULSE_OXIMETER- Heart rate: ");

  Serial.print(average_heart_rate); //display averaged heart rate

  Serial.println(" BPM");


  Serial.println("PULSE_OXIMETER- Blood Oxidation Level:");

  Serial.print(average_SPO2); // display averaged SpO2

  Serial.println("%");

  last_heart_rate = average_heart_rate;
  last_SPO2 = average_SPO2;

  lcd.clear();
  // lcd.setCursor(0,0);
  // lcd.print((String) "HR=" + average_heart_rate);
  // lcd.setCursor(0,1);
  // lcd.print((String) "SPO2=" + average_SPO2 + "%");

  N_DOTS = 0;
  // delay(300);
  // lcd.clear();
}



//Defining function for calculating the average reading

void calc_average(int Heart_rate, int SPO2){ // input parameters to this function will be the current values collected from the sensor

  if (Heart_rate >30 and Heart_rate<220 and SPO2 >50){ //setting threshold on the Heart rate and SpO2 values to be in the expected range

    average_heart_rate = weighted_avg * (Heart_rate) + (1-weighted_avg) * average_heart_rate; //calculate average heart rate  

    average_SPO2 = weighted_avg * (SPO2) + (1-weighted_avg) * average_SPO2; // calculate average SpO2

    pox_readings++; //increment the value of readings (number of beats)

    displayduringcalc(pox_readings); //display Processing

  }

  if(pox_readings == pox_nr_readings){ //check whether the number of collected readings are equal to the number of readings required for averaging

    pox_readings = 0; //reset number of collected readings

    output_display(); // display the heart rate and SpO2 readings

  }

}

void init_pox() {
  pox.begin(); // start sensing from the sensor
  pox.setOnBeatDetectedCallback(beatdetected); // the function of MAX30100 library that collects the signal, computes Heartrate and SpO2 and give output.
  displayinitially();
}

void init_adxl() {
  // Set ADXL345 in measuring mode
  Wire.beginTransmission(ADXL345); // Start communicating with the device 
  Wire.write(0x2D); // Access/ talk to POWER_CTL Register - 0x2D
  // Enable measurement
  Wire.write(8); // (8dec -> 0000 1000 binary) Bit D3 High for measuring enable 
  Wire.endTransmission();

  //Off-set Calibration
  //X-axis
  Wire.beginTransmission(ADXL345);
  Wire.write(0x1E);
  Wire.write(1);
  Wire.endTransmission();
  delay(10);
  //Y-axis
  Wire.beginTransmission(ADXL345);
  Wire.write(0x1F);
  Wire.write(-2);
  Wire.endTransmission();
  delay(10);

  //Z-axis
  Wire.beginTransmission(ADXL345);
  Wire.write(0x20);
  Wire.write(-11);
  Wire.endTransmission();
  delay(10);
}

void init_lcd() {
  lcd.begin(8, 2); 
  lcd.setCursor(0,0);
  lcd.print("Setting");
  lcd.setCursor(0,1);
  lcd.print("Up");
  delay(1000);
  // display heart symbol


  lcd.clear();
}

void get_adxl_reading() {
  Wire.beginTransmission(ADXL345);
  Wire.write(0x32); // Start with register 0x32 (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(ADXL345, 6, true); // Read 6 registers total, each axis value is stored in 2 registers
  ax_out = ( Wire.read()| Wire.read() << 8); // X-axis value
  ax_out = ax_out/256; //For a range of +-2g, we need to divide the raw values by 256, according to the datasheet
  ay_out = ( Wire.read()| Wire.read() << 8); // Y-axis value
  ay_out = ay_out/256;
  az_out = ( Wire.read()| Wire.read() << 8); // Z-axis value
  az_out = az_out/256;
}

void poll_long_press_btn() {
  // read the state of the switch/button:
  currentState = digitalRead(BUTTON_PIN);

  if(lastState == HIGH && currentState == LOW) {        // button is pressed
    pressedTime = millis();
    isPressing = true;
    isLongDetected = false;
  } else if(lastState == LOW && currentState == HIGH) { // button is released
    isPressing = false;
  }

  if(isPressing == true && isLongDetected == false) {
    long pressDuration = millis() - pressedTime;

    if( pressDuration > LONG_PRESS_TIME ) {
      Serial.println("USER: A long press is detected");
      
      // String info;
      // if(average_heart_rate != 0 && average_SPO2 != 0)
      //   info = String("Last HR = ") + String(average_heart_rate) + String(". Last blood_oxidation_level = ") + String(average_SPO2);
      
      String msg = String("Soldier has called for emergency");
      emergency(msg, false);
      
      isLongDetected = true;
    }
  }

  // save the the last state
  lastState = currentState;
}

void poll_pox() {
  pox.update(); //get the updated data

  if ((pox_sampling_time < millis() - last_report_generated)){ // to make sure that the readings are taken within the defined sampling time
      // Serial.println("reading and calc");
     calc_average(pox.getHeartRate(), pox.getSpO2()); // calculate the average values of heart rate and SpO2 using function defined above

     last_report_generated = millis(); // update the time of report generated

  }

  // To make sure the finger is placed of the sensor.

  if ((millis() - last_beat_detected > 3000)){ //if no beat is detected for 3 seconds (3000ms)
      // Serial.println("finger not detected");

    // average_heart_rate = 0; //reset the heart rate values

    // average_SPO2 = 0; //reset the SpO2 values 

    displayinitially(); //Display the user the initial screen asking the user to place the finger on the sensor

  }

}

void poll_adxl() {
  get_adxl_reading();
  float total_acc = sqrt(ax_out*ax_out + ay_out*ay_out + az_out*az_out); 

  if(total_acc <= 0.35 && ((millis() - last_fall_time) > free_fall_debounce_time)) {
    Serial.println((String) "USER: FALL Detected. delta_pos = 5.3m. total_acc=" + total_acc);
    last_fall_time = millis();

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("FALL DET");
    lcd.setCursor(0,1);
    lcd.print((String) "a_t=" + total_acc);
    
    delay(1000);
    emergency((String) "Soldier has fallen with acc=" + total_acc);
    lcd.clear();
  }
}

bool check_confirmation_to_send() {
  Serial.println("USER: Asking soldier for confirmation ...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Deny EM");
 
  unsigned long start_time = millis();
  unsigned long passed = millis() - start_time;
  bool deny = false;

  int btn_val;


  while(passed < BUTTON_CONFIRMATION_TIME) {
    btn_val = digitalRead(BUTTON_PIN);
    int remaining_time =  (int) ((BUTTON_CONFIRMATION_TIME - (millis() - start_time)) / 1000.0);
    lcd.setCursor(0, 1);
    lcd.print((String) "Alert " + remaining_time + "s");

    if(btn_val == LOW) { // pressed
      deny = true;
      Serial.println("USER: EM Alert Denied ...");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("EM Alert");
      lcd.setCursor(0, 1);
      lcd.print("Denied!");

      delay(2000);
      lcd.clear();

      break;
    }

    delay(10);

    passed = millis() - start_time;
  }

  // Serial.println((String) "delay=" + passed);

  lcd.clear();
  return !deny;
}

void emergency(String message, bool confirm) {
  Serial.println();
  Serial.println("USER: Sending EM Alert");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sending");
  lcd.setCursor(0, 1);
  lcd.print("EM alert");
  delay(2000);

  bool send = true;
  if(confirm)
    send = check_confirmation_to_send();
   
  if(send) {
    Serial.println("USER: Microphone is now ON");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MIC ON");
    delay(3000);
    lcd.clear();
    
    Serial.println("USER: Sent emergency alert");
    Serial.println("SERVER: Emergency alert received");
    
    if(last_heart_rate != 0 && last_SPO2 != 0) {
      Serial.print("HR=");
      Serial.print(last_heart_rate);
      Serial.println();
      Serial.print("SPO2=");
      Serial.println(last_SPO2);
      Serial.println();
    }

    Serial.println("EMERGENCY: " + message);
    Serial.println("POSITION: " + GPS_POSITION_STR);

  } else {
    Serial.println("USER: Cancelled emergency alert");
  }
  
  Serial.println();
}

void setup() {
  Serial.begin(9600);
  init_lcd();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(); // Initiate the Wire library
  init_pox();
  init_adxl();
}

void listen_serial_input() {
  if(Serial.available() != 0) {
    int input_sim = Serial.parseInt();
    // Condition:
    // input_sim = 1 -> Unconsciousness
    // input_sim = 2 -> Extremely Tired
    // input_sum = 3 -> Heavy Bleeding
    // String info = "";

    if(input_sim != 0) { // note: 0 maybe sent by default without any input too
      Serial.println();
      Serial.println((String) "Input Simulation: " + input_sim);
      
      // if(average_heart_rate != 0 && average_SPO2 != 0)
      //   info = (String) "Last HR = " + average_heart_rate + (String) ". Last blood_oxidation_level = " + average_SPO2;
    }

    if(input_sim == 1) {
      Serial.println((String) "Input Simulation: " + "Unconsciousness");
      String msg = "Soldier maybe UNCONSCIOUS. ";
      emergency(msg);
    }
    else if(input_sim == 2) {
      Serial.println((String) "Input Simulation: " + "Extreme Fatigue");
      String msg = "Soldier is experiencing EXTREME FATIGUE. ";
      emergency(msg);
    }
    else if(input_sim == 3) {
      Serial.println((String) "Input Simulation: " + "Heavy Bleeding");
      String msg = "Soldier maybe HEAVILY BLEEDING. ";
      emergency(msg);
    }
  }
}

void loop() {
  poll_long_press_btn();

  poll_pox();
  poll_adxl();

  listen_serial_input();
  delay(10);
}