#include <NMEAParser.h>
#include <RCSwitch.h>
#include <TimerOne.h>
#include <ShipGauges.h>

NMEAParser<2> parser;

//enable Debug printing on serial extra info
#define REMOTEDEBUG 0
#define NMEADEBUG 0
#define tx_ST_debug 0
#define BAUDRATE 38400

//Seatalk stuff
#define Seatalk_RX_pin 7
#define Seatalk_TX_pin 6
#define Seatalk_TX_enable 1

#define MAX_SIZE        20
#define MAX_MSGS        20
#define MS_READY         0
#define MS_READING       1
#define MS_DONE          2
#define _12V_state   LOW  //idle state is also on 12V
#define _0V_state    HIGH
#define tx_buffer_size 5

struct Seatalk_buffer
{
  byte statuss;
  byte sizee;
  unsigned char msg[MAX_SIZE];
};

ShipGauges SeaMonitor;
Seatalk_buffer seatalkbuff[MAX_MSGS];
Seatalk_buffer tx_buffer[tx_buffer_size];
boolean seatalk_new_msg = false;
byte ST_idle_count = 0;
boolean ST_TX_sending = false;
boolean ST_line_state = _12V_state;
byte collision_detected = 0;
String s = "";

byte debounce = 250; //ignore if button held down for 250ms
unsigned long debouncetime;

//Set Values below depending on Remote Output
#define PILOTAUTO 5723919
#define PILOTSTANDBY 5724144
#define TILLERPORT10 5723907
#define TILLERSTB10 5723916
#define TILLERPORT1 5723952
#define TILLERSTB1 5724096
#define PORTTACK 5723955
#define STBTACK 5724108
#define WINDMODE 5724099
#define WINDMODE2 5723964

#define PILOTAUTOB 5592380
#define PILOTSTANDBYB 5592515
#define TILLERPORT10B 5592368
#define TILLERSTB10B 5592332
#define TILLERPORT1B 5592512
#define TILLERSTB1B 5592323
#define PORTTACKB 5592560
#define STBTACKB 5592335
#define WINDMODEB 5592371
#define WINDMODE2B 5592524

#define LAMP_OFF 0x00
#define LAMP_ON 0x0C
#define PLUS_ONE 0x07
#define MINUS_ONE 0x05
#define PLUS_TEN 0x08
#define MINUS_TEN 0x06
#define STANDBY 0x02
#define AUTO 0x01
#define WINDTRACK 0x4
#define TRACK 0x03
#define DISP 0x04
#define TACK_MINUS 0x21
#define TACK_PLUS 0x22

byte incomingByte;
byte SEATALK_TX_OUT = 6;
byte SEATALK_RX_IN = 7;
//int cLampState;
bool PilotOn;
#define nopilot "NeedAuto"

RCSwitch mySwitch = RCSwitch();

void setup() {

  SeaMonitor.InitParam();
  seatalk_initialize();
  tx_seatalk_initialize();

  parser.setErrorHandler(errorHandler);
  parser.addHandler("IIMWV", IIMWVHandler);
  parser.addHandler("IIVHW", IIVHWHandler);

  Serial.begin(BAUDRATE);
  mySwitch.enableReceive(0);  // Receiver on inerrupt 0 => that is pin #2
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SEATALK_TX_OUT, OUTPUT); // sets the digital pin as output
  pinMode(SEATALK_RX_IN, INPUT); // sets the digital pin as input

  Serial.println("RedElectronics Seatalk Tiller Remote v1.01 - 12/10/2021");
  Serial.print("NMEA Input Baud: ");
  Serial.println(BAUDRATE);
  if (REMOTEDEBUG){Serial.print("REMOTE DEBUG ON : \t");}else{Serial.print("REMOTE DEBUG OFF : \t");}
  if (NMEADEBUG){Serial.print("NMEA DEBUG ON : \t");}else{Serial.print("NMEA DEBUG OFF : \t");}
  if (tx_ST_debug){Serial.println("ST_TX DEBUG ON");}else{Serial.println("ST_TX DEBUG OFF");}
}

void loop() {

  seatalk_process_new_messages();
  delay(1);

//If NMEA Data incoming, then Parse it...
if (Serial.available()) {
    parser << Serial.read();
  }

  if (mySwitch.available()) {
    
  long value = mySwitch.getReceivedValue();

  if(REMOTEDEBUG) Serial.println( value ); // Print out Value of FOB, used in Setup 
  
//debounce the remote so holding down a button doesnt produce a massive direction changes
if (((millis()-debouncetime) > debounce) )  //
 {   
  debouncetime = millis();
  
 switch (value)
    {
      case PILOTAUTO: // FOB Buttons A&B Pressed together to give "Auto"
        PilotOn = 1;
        digitalWrite(LED_BUILTIN, PilotOn);
        Serial.print( "A:\t" ); // 
        ST_send_AUTO();
        break;
        case PILOTAUTOB: // FOB Buttons A&B Pressed together to give "Auto"
        PilotOn = 1;
        digitalWrite(LED_BUILTIN, PilotOn);
        Serial.print( "AB:\t" ); // 
        ST_send_AUTO();
        break;
        
      case PILOTSTANDBY:
      PilotOn = 0;
        digitalWrite(LED_BUILTIN, PilotOn); // FOB Buttons A&C Pressed together to give "Standby"
        Serial.print( "S:\t" );
        ST_send_STANDBY();
        break;
        case PILOTSTANDBYB:
        PilotOn = 0;
        digitalWrite(LED_BUILTIN, PilotOn); // FOB Buttons A&C Pressed together to give "Standby"
        Serial.print( "SB:\t" );
        ST_send_STANDBY();
        break;
      
      case TILLERPORT10:
         if( PilotOn ) {// if Auto Enabled 
         // FOB Button A Pressed to give "-10 DEG"
        Serial.print( "P10:\t" ); 
        ST_send_minus10();
        }else{
        if (REMOTEDEBUG) Serial.println( nopilot ); 
        }
        break;
        case TILLERPORT10B:
         if( PilotOn ) {// if Auto Enabled 
         // FOB Button A Pressed to give "-10 DEG"
        Serial.print( "P10B:\t" ); 
        ST_send_minus10();
        }else{
       if (REMOTEDEBUG) Serial.println( nopilot ); 
        }
        break;
        
      case TILLERSTB10:
        if( PilotOn ) {// if Auto Enabled 
        // FOB Button B Pressed to give "+10 DEG"
        Serial.print( "S10:\t" );
        ST_send_plus10();
        }else{
        if (REMOTEDEBUG) Serial.println( nopilot); 
        }
        break;
        case TILLERSTB10B:
        if( PilotOn ) {// if Auto Enabled 
        // FOB Button B Pressed to give "+10 DEG"
        Serial.print( "S10B:\t" );
        ST_send_plus10();
        }else{
       if (REMOTEDEBUG) Serial.println( nopilot ); 
        }
        break;
        
       case TILLERPORT1:
       if(PilotOn ) {// if Auto Enabled
         // FOB Button C Pressed to give "-1 Deg"
        Serial.print( "P1:\t" ); 
        ST_send_minus1();
        }else{
        if (REMOTEDEBUG) Serial.println( nopilot ); 
        }
        break;
        case TILLERPORT1B:
       if(PilotOn ) {// if Auto Enabled
         // FOB Button C Pressed to give "-1 Deg"
        Serial.print( "P1B:\t" ); 
        ST_send_minus1();
        }else{
        if (REMOTEDEBUG) Serial.println( nopilot ); 
        }
        break;
        
      case TILLERSTB1:
      if( PilotOn ) {// if Auto Enabled
        // FOB Button D Pressed to give "+1 Deg"
        Serial.print( "S1:\t" ); 
        ST_send_plus1();
        }else{
        if (REMOTEDEBUG) Serial.println( nopilot ); 
        }
        break;
        case TILLERSTB1B:
      if( PilotOn ) {// if Auto Enabled
        // FOB Button D Pressed to give "+1 Deg"
        Serial.print( "S1B:\t" ); 
        ST_send_plus1();
        }else{
        if (REMOTEDEBUG) Serial.println( nopilot ); 
        }
        break;
        
       case PORTTACK:
       if( PilotOn ) {// if Auto Enabled
        // FOB Button A+C Pressed to give "Tack Left"
        Serial.print( "PT:\t" ); 
        ST_send_PORTTACK();
        }else{
        if (REMOTEDEBUG) Serial.println( nopilot ); 
        }
        break;
        case PORTTACKB:
       if( PilotOn ) {// if Auto Enabled
        // FOB Button A+C Pressed to give "Tack Left"
        Serial.print( "PTB:\t" ); 
        ST_send_PORTTACK();
        }else{
        if (REMOTEDEBUG) Serial.println( nopilot ); 
        }
        break;
        
      case STBTACK:
      if( PilotOn ) {// if Auto Enabled
         // FOB Button B+D Pressed to give "Tack Right"
        Serial.print( "ST:\t" ); 
        ST_send_STBTACK();
        }else{
        if (REMOTEDEBUG) Serial.println( nopilot ); 
        }
        break;
        case STBTACKB:
      if( PilotOn ) {// if Auto Enabled
         // FOB Button B+D Pressed to give "Tack Right"
        Serial.print( "STB:\t" ); 
        ST_send_STBTACK();
        }else{
        if (REMOTEDEBUG) Serial.println( nopilot ); 
        }
        break;
        
      case WINDMODE:
        if( PilotOn ) {// if Auto Enabled 
        Serial.print( "W:\t" ); // FOB Button A+D Pressed to give "Wind Mode" same as below
        ST_send_WINDMODE();
        }else{
        if (REMOTEDEBUG)Serial.println( nopilot ); 
        }
        break;
        case WINDMODEB:
        if( PilotOn ) {// if Auto Enabled 
        Serial.print( "WB:\t" ); // FOB Button A+D Pressed to give "Wind Mode" same as below
        ST_send_WINDMODE();
        }else{
        if (REMOTEDEBUG)Serial.println( nopilot ); 
        }
        break;
        
      case WINDMODE2:
        if( PilotOn ) {// if Auto Enabled
        Serial.print( "W2:\t" ); // FOB Button C+B Pressed to give "Wind Mode" same as above
        ST_send_WINDMODE();
        }else{
        if (REMOTEDEBUG) Serial.println( nopilot ); 
        }
        break;
        case WINDMODE2B:
        if( PilotOn ) {// if Auto Enabled
        Serial.print( "W2B:\t" ); // FOB Button C+B Pressed to give "Wind Mode" same as above
        ST_send_WINDMODE();
        }else{
        if (REMOTEDEBUG) Serial.println( nopilot ); 
        }
        break;
        
     // default:
      if (REMOTEDEBUG) Serial.println("no match");
     
    }
 }
   mySwitch.resetAvailable();
  }
      if (s.length() > 0)
  {
    String ys = s;
    s = "";
    Serial.print(ys);
  }
}  



void IIMWVHandler()
{
 //we only need fields 1 and 3
  float arg0;
  float arg1;
  parser.getArg(0,arg0); //get Wind angle
  parser.getArg(2,arg1); //get Wind speed
  
   
  if (NMEADEBUG) {
    Serial.print("MWV Wind: #");
    Serial.print(parser.argCount());
    Serial.print(" : ST_send_aws: ");
    Serial.print(arg1);
    Serial.print(" : ST_send_awa: ");
    Serial.println(arg0);
    }  
    
  ST_send_aws(arg1); 
  ST_send_awa(arg0);
}

void IIVHWHandler()
{
  //we only need data field 5  (which is 4 as starts at 0)
  float arg1;
  parser.getArg(4,arg1); //get Speed
   
  if (NMEADEBUG) {
    Serial.print("VHW Speed: #");
    Serial.print(parser.argCount());
    Serial.print(" : ST_send_boat_speed: ");
    Serial.println(arg1);
    }
  ST_send_boat_speed(arg1);  
  }

  void errorHandler()
{
    Serial.print("NMEA Err: "); 
    Serial.println(parser.error()); 
  
  }
