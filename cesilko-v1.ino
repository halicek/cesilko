/* Big portion of code base on Arduino tutorial codes (LCD crystal, rotary encoder, EEPROM read and write).
 * TODO: Set backlight to be controlled by the program (via PWM on LCD pin A) -- not going to happen :/ (maybe ver 2 on another pcb)
*/


#include <LiquidCrystal.h>
#include <EEPROM.h>

// pin numbers
// TODO: UPDATE
#define switch_black A3
#define button_up 8
#define button_down 8
#define button_red 9
#define relay 10
#define led 13 //internal led //TODO: Zapojit externí ledky?

// finite state machine states
#define S_NORMAL 0
#define S_AUTOPUMP 1
#define S_MANPUMP 2
#define S_SET_PUMP_M 3
#define S_SET_PUMP_S 4
#define S_SET_ALARM_H 5
#define S_SET_ALARM_M 6

// states
uint8_t status = 0; // FSM status variable, 0 = default (automatic countdown mode)
// TODO: FIX THIS (switch black state not exicitng)
uint8_t switch_black_state = HIGH;
uint8_t button_up_state = 0; 
uint8_t button_down_state = 0;
uint8_t button_red_state = 0;

// LCD custom characters - ducks incoming!
// http://maxpromer.github.io/LCD-Character-Creator/
uint8_t bell[8]  = {0x4,0xe,0xe,0xe,0x1f,0x0,0x4};
uint8_t clock[8] = {0x0,0xe,0x15,0x17,0x11,0xe,0x0};
uint8_t duck[8]  = {0x0,0xc,0x1d,0xf,0xf,0x6,0x0};

// initializing LCD 1602A with Hitachi HD44780 driver
const int rs = 12, en = 11, d4 = 2, d5 = 3, d6 = 4, d7 = 5;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// default user time values - everything in SECONDS
//TODO: Set reasonable initial times
long running_time = 0L; // how far we are in the current interval
long alarm_time = 120L; //when to start the pump - how long the interval is
long pumping_time = 20L; // how long should pump run
long pumping_time_remaining = 0L; // how much time of pumping remains
long manual_time = 0L; // how long does pump run on manual override

//TODO: clean
const long freq_sec = 1000;
const long freq_min = 60000;
unsigned long previousMillis = 0; 


// rotary encoder variables
int val;
int encoderPinA = 7;
int encoderPinB = 9;
int encoderPinPush = A1; 
int encoderPos=0;
int encoderPinALast = HIGH;
int statusA = LOW;
int statusB = LOW;

// writing to EEPROM
int lastpush=0;
int eepromAddr=0;

// to mark time when the push of the button started.
unsigned long pushStartMillis = 0L;


void setup() {
  // serial init
  Serial.begin(9600);

   // lcd init
  lcd.begin(16, 2);

  lcd.createChar(0, bell);
  lcd.createChar(1, clock);
  lcd.createChar(2, duck);

  pinMode(led, OUTPUT);
  pinMode(button_up,INPUT_PULLUP); //https://www.arduino.cc/en/Tutorial/DigitalPins
  pinMode(button_down,INPUT_PULLUP);
  pinMode(button_red, INPUT_PULLUP);
  pinMode(switch_black, INPUT_PULLUP);
  pinMode(relay, OUTPUT);
  pinMode(encoderPinA, INPUT_PULLUP);
  pinMode(encoderPinB, INPUT_PULLUP);
  pinMode(encoderPinPush, INPUT_PULLUP);

  alarm_time = EEPROMReadlong(eepromAddr);
  pumping_time = EEPROMReadlong(eepromAddr+4);
  
}

/* Function to format time into string like " 4h22m" or "12h05m"
 * output - string (pointer to string) with formatted output
 * time - time to format
*/
void formatTimeH(char output[7], long time){

  if (time>=36000L){
    output[0] = (time / 36000L)+48L;
  } else{
    output[0] = ' ';
  }

  if (time>=3600L){
    output[1] = ((time%36000L)/3600L)+48L;
  } else{
    output[1] = '0';
  }

  output[2] = 'h';
  
  if (time>=600L){
    long tens = 0;
    tens = (((time%36000L)%3600L)/600L)%360L; //because we do not have "one hour seventy four minutes"
    output[3] = tens+48L;
    } else{
    output[3] = '0';
  }

  if (time>=60L){
    output[4] = ((((time%36000L)%3600L)%600L)/60L)+48L;
  } else{
    output[4] = '0';
  }

  output[5] = 'm';
  output[6] = '\0';
}

/* Function to format time into string like " 4m22s" or "12m05s"
 * output - string (pointer to string) with formatted output
 * time - time to format
*/
void formatTimeM(char output[7], long time){

  if (time>=600L){
    output[0] = (((time%36000L)%3600L)/600L)+48L;
  } else{
    output[0] = ' ';
  }

  if (time>=60L){
    output[1] = ((((time%36000L)%3600L)%600L)/60L)+48L;
  } else{
    output[1] = '0';
  }

  output[2] = 'm';
  
  if (time>=10L){
    long tens = 0;
    tens = ((((time%36000L)%3600L)%600L)/10L)%6L; //because we do not have "one minute seventy four seconds"
    output[3] = tens+48L;
  } else{
    output[3] = '0';
  }
  
  output[4] = (time%10L)+48L;
  
  output[5] = 's';
  output[6] = '\0';
}

void printStatus(int status){
  
  if (status==S_NORMAL){
    lcd.clear();
    lcd.print("Automat   ");
    lcd.setCursor(9,0);
    lcd.write((byte)1); // clock
    lcd.setCursor(10,0);
    char running[7];
    formatTimeH(running,running_time);
    lcd.print(running);
    lcd.setCursor(0,1);
    lcd.write((byte)2);//duck
    lcd.setCursor(1,1);
    char pump[7];
    formatTimeM(pump,pumping_time);
    lcd.print(pump);
    //TODO fix the missing spaces not written
    lcd.setCursor(7,1);
    lcd.print("  ");
    lcd.setCursor(9,1);
    lcd.write((byte)0);
    lcd.setCursor(10,1);
    char alarm[7];
    formatTimeH(alarm,alarm_time);
    lcd.print(alarm);
  } 
  else if (status==S_AUTOPUMP){
    lcd.clear();
    lcd.print("Cerpadlo bezi!!!");
    lcd.setCursor(0,1);
    lcd.write((byte)2);//duck
    lcd.setCursor(1,1);
    char remaining[7];
    formatTimeM(remaining,pumping_time_remaining);
    lcd.print(remaining);
  } 
  else if (status==S_MANPUMP){
    lcd.clear();
    lcd.print("! Manualni beh !");
    lcd.setCursor(0,1);
    lcd.print("!    ");
    lcd.setCursor(5,1);
    char manual[7];
    formatTimeM(manual,manual_time);
    lcd.print(manual);
    lcd.setCursor(15,1);
    lcd.print("!");//duck
  }  
}


//This function will write a 4 byte (32bit) long to the eeprom at
//the specified address to address + 3.
void EEPROMWritelong(int address, long value)
      {
      //Decomposition from a long to 4 bytes by using bitshift.
      //One = Most significant -> Four = Least significant byte
      byte four = (value & 0xFF);
      byte three = ((value >> 8) & 0xFF);
      byte two = ((value >> 16) & 0xFF);
      byte one = ((value >> 24) & 0xFF);

      //Write the 4 bytes into the eeprom memory.
      EEPROM.write(address, four);
      EEPROM.write(address + 1, three);
      EEPROM.write(address + 2, two);
      EEPROM.write(address + 3, one);
}

long EEPROMReadlong(long address)
      {
      //Read the 4 bytes from the eeprom memory.
      long four = EEPROM.read(address);
      long three = EEPROM.read(address + 1);
      long two = EEPROM.read(address + 2);
      long one = EEPROM.read(address + 3);

      //Return the recomposed long by using bitshift.
      return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
      }


void loop() {
   
  unsigned long currentMillis = millis();

  statusA = digitalRead(encoderPinA);
  statusB = digitalRead(encoderPinB);
  int pushed = digitalRead(encoderPinPush);

  if ((lastpush==0)&&(pushed==LOW)){
      EEPROMWritelong(eepromAddr, alarm_time);
      EEPROMWritelong(eepromAddr+4, pumping_time);
      lastpush=1;
  }
  if (pushed==HIGH){
      
      lastpush=0;
    }
    
  Serial.print(".");
  if ((encoderPinALast == LOW) && (statusA == HIGH)) {
    if (pushed==HIGH){
      if (statusB == LOW) {
      alarm_time-=300;
      if (alarm_time<300) alarm_time=300;
      } else {
        alarm_time+=300;
      }
    }
    else{
        if (statusB == LOW) {
          pumping_time-=10;
          if (pumping_time<10) pumping_time=10;
        }
        else{
          pumping_time+=10;
          }
      }
  }
  encoderPinALast = statusA;

  switch_black_state = digitalRead(switch_black);
  // Checking the manual override
  if (switch_black_state == LOW){
    Serial.println("LOOOOW!");
    if (status!=S_MANPUMP){
      // first time we hit a manual mode
      manual_time=0;
      digitalWrite(relay, HIGH);
      status=S_MANPUMP;
    }
  }
  else {
    if (status==S_MANPUMP){
      // we are exiting manual mode
      digitalWrite(relay, LOW);
      status=S_NORMAL;
      running_time=0L;
      pumping_time_remaining = pumping_time;
    }
  }

  /*
   * The main state changes happe on whole seconds ...
   * also, this solves millis() rollover/overflow (each cca 49+ days)
   */
  if (currentMillis - previousMillis >= freq_sec) {
    // save the last time you did this
    previousMillis = currentMillis;
      
    if (status==S_NORMAL){  
      running_time++;
      printStatus(S_NORMAL);
      
      if (running_time>=alarm_time){
        status=S_AUTOPUMP; // switching to auto pump mode
        digitalWrite(relay, HIGH);
        pumping_time_remaining = pumping_time;
        running_time=0L; //resetting running time for the next loop
      }
    }
    else if (status==S_AUTOPUMP){
      pumping_time_remaining--;
      printStatus(S_AUTOPUMP);

      if (pumping_time_remaining <=0L){
        status=S_NORMAL; // switching to auto pump mode
        digitalWrite(relay, LOW);
      }
      
    }
    else if (status==S_MANPUMP){
      manual_time++;
      printStatus(S_MANPUMP);
    }


      
  }   
