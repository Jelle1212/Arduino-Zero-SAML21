#include <Arduino.h>
#include <Controllino.h>
#include <SPI.h>
#include <avr/wdt.h>
#include <EEPROM.h>

unsigned long startMillis =0;  //some global variables available anywhere in the program
unsigned long currentMillis = 0;
uint32_t period_1 = 300000;  //the value is a number of minutes 5
uint32_t period_2 = 120000;  //the value is a number of minutes 2

uint8_t lamp = 0;
uint8_t stop = 0;
uint16_t rpm_pump = 2000;
float setpoint_temperature=0;
float sensor_temperature=0;

uint8_t rpm_eeprom;
uint8_t time1_eeprom;
uint8_t time2_eeprom;

void parseData(void);
void recvWithStartEndMarkers(void);
void showData(void);
void pump_rpm(void);
void everything_off(void);
void reset_bool_pump(void);
void eeprom_update(void);
void eeprom_write(void);
void eeprom_read(void);

volatile uint8_t scenario = 0;

bool message_water_level_low = false;
bool message_water_level_ok = false;
bool message_heatpump_warning = false;
bool message_heatpump_error = false;
bool message_water_drained = false;

bool button_flag_fill = false;
bool button_flag_empty = false;

bool pomp_600 = false;
bool pomp_2000 = false;
bool pomp_3000 = false;
bool pomp_stop = false;

bool flag_done_low_600rpm = false;
bool flag_done_controlling = false;

bool stop_all = false;

bool flag_low_floater_was_high = false;

boolean newData = false;

const byte numChars = 32;
char receivedChars[numChars];   // an array to store the received data
char tempChars[numChars];        // temporary array for use when parsing
char command[numChars] = {0};

char command_temp[] = "T"; // temperature
char command_set[] = "S";  // Setpoint
char command_rt[] = "L";   // Lamp
char command_rpm[] = "R"; // RPM pump
char command_period1[] = "A"; //minuter period 1
char command_period2[] = "B"; //minuter period 2
char command_stop_pump[] = "P"; // stop pump
char command_empty[] = "X"; // empty swimmingpool
char command_fill[] = "F"; // fill swimmingpool
char command_error_list[] = "E"; // list of error
char command_status_list[] = "U"; // list of status

uint8_t address_rpm_pump = 0x00;
uint8_t address_period_1 = 0x01;
uint8_t address_period_2 = 0x02;

void setup() {
    pinMode(CONTROLLINO_D0, OUTPUT); // pump stop
    pinMode(CONTROLLINO_D1, OUTPUT); // pump 600 rpm
    pinMode(CONTROLLINO_D2, OUTPUT); // pump 2000 rpm
    pinMode(CONTROLLINO_D3, OUTPUT); // pump 3000 rpm 
    pinMode(CONTROLLINO_D4, OUTPUT); // cool amezon & heatpump
    pinMode(CONTROLLINO_D5, OUTPUT); // heater
    pinMode(CONTROLLINO_D6, OUTPUT); // light bulb
    pinMode(CONTROLLINO_D7, OUTPUT); // valve
    pinMode(CONTROLLINO_A0, INPUT);  // float low
    pinMode(CONTROLLINO_A1, INPUT);  // float high
    pinMode(CONTROLLINO_A2, INPUT);  // on/off
    pinMode(CONTROLLINO_A3, INPUT);  // cooling/heating control
    pinMode(CONTROLLINO_IN0, INPUT); // button backwah/empty
    pinMode(CONTROLLINO_IN1, INPUT); // button fill
    startMillis = millis();  //initial start time
    //eeprom_write();
    eeprom_read();
    Serial.begin(9600);
    wdt_enable(WDTO_8S);
}

void loop() {
  
  while(scenario == 0){

  wdt_reset();
  recvWithStartEndMarkers();
  if (newData == true) {
      strcpy(tempChars, receivedChars);
          // this temporary copy is necessary to protect the original data
          //   because strtok() used in parseData() replaces the commas with \0
      parseData();
      eeprom_update();
      eeprom_read();
      newData = false;
  }

  if(digitalRead(CONTROLLINO_A2) == true){ // is the switch on
    pump_rpm();
    pomp_stop = false;
    if(digitalRead(CONTROLLINO_A3) == true){ // wished to control temp
      reset_bool_pump();
      scenario = 1;
      break;
    }
  }
  
  if(digitalRead(CONTROLLINO_A2) == false || stop == 1){ // is the switch off
    everything_off();
    pomp_2000 = false;
    pomp_3000 = false;
    pomp_600 = false;
    if(digitalRead(CONTROLLINO_IN0) == true){ // wished for backwas/empty
      reset_bool_pump();
      scenario = 2;
      break;
    }

    else if(digitalRead(CONTROLLINO_IN1) == true){ // wished to fill
      scenario = 3;
      break;
      }
    }
  }
  
  while(scenario == 1){ //wish to control temp
  wdt_reset();
    recvWithStartEndMarkers();
    if (newData == true) {
        strcpy(tempChars, receivedChars);
            // this temporary copy is necessary to protect the original data
            //   because strtok() used in parseData() replaces the commas with \0
        parseData();
        eeprom_update();
        eeprom_read();
        newData = false;
    }
    pump_rpm(); //check the floats and change the rpm

    if(lamp){
        digitalWrite(CONTROLLINO_D6, HIGH);  // lamp
    }

    else{
        digitalWrite(CONTROLLINO_D6, LOW);  // lamp
    }

    if((sensor_temperature - setpoint_temperature) > 0.1){
      digitalWrite(CONTROLLINO_D4, HIGH);  // cool amezon & heatpump
      digitalWrite(CONTROLLINO_D5, LOW);   // heater
    }

    if((sensor_temperature - setpoint_temperature) < -0.1){
      digitalWrite(CONTROLLINO_D4, LOW);   // cool amezon & heatpump
      digitalWrite(CONTROLLINO_D5, HIGH);  // heater
    }

    if(sensor_temperature == setpoint_temperature){
        flag_done_controlling = true;
    }

    if (flag_done_controlling == true)
    {
      if((( sensor_temperature - setpoint_temperature) >= -0.1 && 0.1 >= ( sensor_temperature - setpoint_temperature)) && !stop){
            digitalWrite(CONTROLLINO_D4, LOW);  // cool amezon & heatpump
            digitalWrite(CONTROLLINO_D5, LOW);  // heater
            flag_done_controlling = false;
        }
    }

    if (digitalRead(CONTROLLINO_A2) == false || stop == 1){ // if stop command is issued or the off swith is active, break loop
      scenario = 0;
      break;
    }
  }
    
  while(scenario == 2){ // wish to backwas
  wdt_reset();
  if(pomp_2000 == false){
        digitalWrite(CONTROLLINO_D1, LOW);  // 600 rpm
        digitalWrite(CONTROLLINO_D2, HIGH);  // 2000 rpm
        delay(100);
        digitalWrite(CONTROLLINO_D2, LOW);  // 2000 rpm
        digitalWrite(CONTROLLINO_D3, LOW);  // 3000 rpm
        pomp_600 = false;
        pomp_2000 = true;
        pomp_3000 = false;
        pomp_stop = false;
    }
    currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
    if(currentMillis - startMillis >= period_2 ){  // 5 minute delay => off => message done
      if(pomp_stop == false){
        digitalWrite(CONTROLLINO_D0, HIGH);
        delay(100);
        digitalWrite(CONTROLLINO_D0, LOW);
        pomp_stop = true;
      }
    startMillis = currentMillis;  //IMPORTANT reset
    break;
    } 
  }

  while(scenario == 3){
    wdt_reset();
    if((digitalRead(CONTROLLINO_A0) == true) && (digitalRead(CONTROLLINO_A1) == false)){
      digitalWrite(CONTROLLINO_D7, HIGH);  // open valve
    }
    else{
      digitalWrite(CONTROLLINO_D7, LOW);  // open valve
      if(message_water_level_ok == false){
        Serial.println("water level ok");
        message_water_level_ok = true;
        message_water_level_low = false;
      }
      scenario = 0;
      break;
    }
  }

}
  


void pump_rpm(void){
  
  if(digitalRead(CONTROLLINO_A0) == false && digitalRead(CONTROLLINO_A1) == true){ //float high active

    if(pomp_600 == false){ // float low and high active (inverse of floater low)
        digitalWrite(CONTROLLINO_D1, HIGH);  // 600 rpm
        delay(500);
        digitalWrite(CONTROLLINO_D1, LOW);  // 600 rpm
        digitalWrite(CONTROLLINO_D2, LOW);  // 2000 rpm
        digitalWrite(CONTROLLINO_D3, LOW);  // 3000 rpm
        pomp_600 = true;
        pomp_2000 = false;
        pomp_3000 = false;
        pomp_stop = false;
    }
  }

  else if(digitalRead(CONTROLLINO_A1) == false && digitalRead(CONTROLLINO_A0) == false){  //float high non active
      if(message_water_level_ok == false){
        Serial.println("water level ok");
        message_water_level_ok = true;
        message_water_level_low = false;
      }
      if(pomp_2000 == false && rpm_pump == 2000){
        digitalWrite(CONTROLLINO_D1, LOW);  // 600 rpm
        digitalWrite(CONTROLLINO_D2, HIGH);  // 2000 rpm
        delay(500);
        digitalWrite(CONTROLLINO_D2, LOW);  // 2000 rpm
        digitalWrite(CONTROLLINO_D3, LOW);  // 3000 rpm
        pomp_600 = false;
        pomp_2000 = true;
        pomp_3000 = false;
        pomp_stop = false;
        flag_low_floater_was_high = true;
      }

      else if(pomp_3000 == false && rpm_pump == 3000){
        digitalWrite(CONTROLLINO_D1, LOW);  // 600 rpm
        digitalWrite(CONTROLLINO_D2, LOW);  // 2000 rpm
        digitalWrite(CONTROLLINO_D3, HIGH);  // 3000 rpm
        delay(500);
        digitalWrite(CONTROLLINO_D3, LOW);  // 3000 rpm
        pomp_600 = false;
        pomp_2000 = false;
        pomp_3000 = true;
        pomp_stop = false;
        flag_low_floater_was_high = true;
      }
    }
  
  else if((digitalRead(CONTROLLINO_A0) == true) && (digitalRead(CONTROLLINO_A1) == false)){ //float low is false
        if(message_water_level_low == false){
            Serial.println("water level low");
            message_water_level_low = true;
            message_water_level_ok = false;
        }
        if(flag_low_floater_was_high == true){// but it was high
          currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
          if(flag_done_low_600rpm == false){
            digitalWrite(CONTROLLINO_D1, HIGH);  // 600 rpm
            delay(500);
            digitalWrite(CONTROLLINO_D1, LOW);  // 600 rpm
            digitalWrite(CONTROLLINO_D2, LOW);  // 2000 rpm
            digitalWrite(CONTROLLINO_D3, LOW);  // 3000 rpm
            flag_done_low_600rpm = true;
            pomp_600 = false;
            pomp_2000 = false;
            pomp_3000 = false;
          }

          if (currentMillis - startMillis >= period_1){  // 5 minute delay => off => message done

              flag_done_low_600rpm = false;
              if(pomp_stop == false){
                digitalWrite(CONTROLLINO_D0, HIGH);
                delay(500);
                digitalWrite(CONTROLLINO_D0, LOW);
                flag_low_floater_was_high = false;
                pomp_stop = true;
              }
              startMillis = currentMillis;  //IMPORTANT reset
              
            }   
        }

       else if(flag_low_floater_was_high == false){
         if(pomp_stop == false){
           digitalWrite(CONTROLLINO_D0, HIGH);
           delay(500);
           digitalWrite(CONTROLLINO_D0, LOW);
           pomp_stop = true;
         }
       }       
  } 
}

void everything_off(void){
  if(pomp_stop == false){
    digitalWrite(CONTROLLINO_D0, HIGH);
    delay(100);
    digitalWrite(CONTROLLINO_D0, LOW);
    digitalWrite(CONTROLLINO_D1, LOW);  // 600 rpm
    digitalWrite(CONTROLLINO_D2, LOW);  // 2000 rpm
    digitalWrite(CONTROLLINO_D3, LOW);  // 3000 rpm
    pomp_stop = true;
  }
  digitalWrite(CONTROLLINO_D4, LOW);  // amezon & heatpump off
  digitalWrite(CONTROLLINO_D5, LOW);  // heater off
  digitalWrite(CONTROLLINO_D7, LOW);  // valve off
}

void parseData() {      // split the data into its parts

    char * strtokIndx; // this is used by strtok() as an index

    strtokIndx = strtok(tempChars,",");      // get the first part - the string
    strcpy(command, strtokIndx); // copy it to messageFromPC

    if(strcmp(command, command_temp) == 0){ //compare char* & char*
        strtokIndx = strtok(NULL, ",");
        sensor_temperature = atof(strtokIndx);         // convert this part to a float
    }

    if(strcmp(command, command_set) == 0){
        strtokIndx = strtok(NULL, ",");
        setpoint_temperature = atof(strtokIndx);         // convert this part to a float
    }

    if(strcmp(command, command_rt) == 0){
        strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
        lamp = atoi(strtokIndx);     // convert this part to an integer
        
    }

    if(strcmp(command, command_rpm) == 0){
        strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
        rpm_eeprom= atoi(strtokIndx);     // convert this part to an integer
    }

    if(strcmp(command, command_period1) == 0){
        strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
        time1_eeprom= atoi(strtokIndx);     // convert this part to an integer
    }

    if(strcmp(command, command_period2) == 0){
        strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
        time2_eeprom= atoi(strtokIndx);     // convert this part to an integer
    }

    if(strcmp(command, command_stop_pump) == 0){
        strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
        stop = atoi(strtokIndx);     // convert this part to an integer
    }

    if(strcmp(command, command_error_list) == 0){
        if (message_water_level_low == false){
            Serial.print("0, ");
        }

        if(message_water_level_low == true){
            Serial.print("1, ");
            
        }

        if (message_water_level_ok == false){
            Serial.print("0, ");
        }

        if(message_water_level_ok == true){
            Serial.print("1, ");
            
        }

        if (message_water_drained == false){
            Serial.print("0, ");
        }

        if(message_water_drained == true){
            Serial.print("1, ");
            message_water_drained = false;
        }

    }

    if(strcmp(command, command_status_list) == 0){
        showData();
    }
    if(strcmp(command, command_empty) == 0){
        button_flag_empty = true;
    }
    if(strcmp(command, command_fill) == 0){
        button_flag_fill = true;
    }
    
}

void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;

    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();

        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}

void showData(){
    Serial.print(setpoint_temperature);
    Serial.print(", ");
    Serial.print(sensor_temperature);
    Serial.print(", ");
    Serial.print(lamp);
    Serial.print(", ");
    Serial.print(stop);
    Serial.print(", ");
    Serial.print(rpm_pump);
    Serial.print(", ");
    Serial.print(rpm_eeprom);
    Serial.print(", ");
    Serial.print(period_1);
    Serial.print(", ");
    Serial.print(time1_eeprom);
    Serial.print(", ");
    Serial.print(period_2);
    Serial.print(", ");
    Serial.print(time2_eeprom);
    Serial.print("\n");        
}

void reset_bool_pump(void){ // reseting bools
  pomp_600 = false;
  pomp_2000 = false;
  pomp_3000 = false;
  pomp_stop = false;
}

void eeprom_update(void){
  EEPROM.update(address_rpm_pump, rpm_eeprom);
  EEPROM.update(address_period_1, time1_eeprom);
  EEPROM.update(address_period_2, time2_eeprom);
}

void eeprom_write(void){
  EEPROM.write(address_rpm_pump, 0x02);
  EEPROM.write(address_period_1, 0x02);
  EEPROM.write(address_period_2, 0x05);
}

void eeprom_read(void){
  rpm_eeprom = EEPROM.read(address_rpm_pump);
  rpm_pump = rpm_eeprom * 1000;
  time1_eeprom = EEPROM.read(address_period_1);
  period_1 = time1_eeprom * 60000;
  time2_eeprom = EEPROM.read(address_period_2);
  period_2 = time2_eeprom * 60000;
}