#define SEATALK_SERIAL_DEBUG 1

// these are Seatalk message command identifiers
#define SEATALK_ID_DEPTH 					0x00
#define SEATALK_ID_BOATSPEED					0x20
#define SEATALK_ID_VARIATION					0x99
#define SEATALK_ID_TEMPERATURE					0x27
#define SEATALK_ID_TRIP						0x21
#define SEATALK_ID_LOG						0x22
#define SEATALK_ID_TRIPLOG					0x25
#define SEATALK_ID_APPARENT_WIND_ANGLE 			        0x10
#define SEATALK_ID_APPARENT_WIND_SPEED 			        0x11
#define SEATALK_ID_SOG						0x52
#define SEATALK_ID_COG						0x53
#define SEATALK_ID_LATITUDE					0x50
#define SEATALK_ID_LONGITUDE					0x51
#define SEATALK_ID_GMT						0x54
#define	SEATALK_ID_DATE						0x56
#define SEATALK_ID_NAV_TO_WAYPOINT				0x85

unsigned int v = 0;
void seatalk_process_new_messages()
{
  if (seatalk_new_msg == true)
  {
    unsigned long millisnow = millis();
    seatalk_new_msg = false;
    for (v = 0; v < MAX_MSGS; v++)
    {
      // now look at all the messages in the messages list to see if there are any new ones ready to processing
      if (seatalkbuff[v].statuss == MS_READY)
      {
        unsigned char message_id;
        seatalkbuff[v].statuss = MS_READING;
        if (SEATALK_SERIAL_DEBUG)
        {
          Serial.print("\tST RX:\t");
          for (int j = 0; j < MAX_SIZE; j++)
          {
            Serial.print(seatalkbuff[v].msg[j], HEX);
            Serial.print(' ');
            if (seatalkbuff[v].sizee == j)
            {
              Serial.println();
              break;
            }
          }
        }

        message_id = seatalkbuff[v].msg[0];



        // seatalk add more message types here
        seatalkbuff[v].statuss = MS_DONE;
      }
    }
  }
}
