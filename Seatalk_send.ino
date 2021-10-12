

//TX states
#define TX_EMPTY 0
#define TX_READY_TO_SEND 10
#define TX_SENDING 9

int anchor_byte = 0;
int anchor_bit = 0;
int anchor_msg = 0;

void ST_send_minus1()
{
  unsigned char datag[2];
  datag[0] = 0x05;
  datag[1] = 0xFA;
  tx_load_seatalk_msg(0x86, 0x11, datag, 2);
}

void ST_send_minus10()
{
  unsigned char datag[2];
  datag[0] = 0x06;
  datag[1] = 0xF9;
  tx_load_seatalk_msg(0x86, 0x11, datag, 2);
}

void ST_send_plus1()
{
  unsigned char datag[2];
  datag[0] = 0x07;
  datag[1] = 0xF8;
  tx_load_seatalk_msg(0x86, 0x11, datag, 2);
}

void ST_send_plus10()
{
  unsigned char datag[2];
  datag[0] = 0x08;
  datag[1] = 0xF7;
  tx_load_seatalk_msg(0x86, 0x11, datag, 2);
}

void ST_send_AUTO()
{
  unsigned char datag[2];
  datag[0] = 0x01;
  datag[1] = 0xfe;
  tx_load_seatalk_msg(0x86, 0, datag, 2);
}

void ST_send_STANDBY()
{
  unsigned char datag[2];
  datag[0] = 0x02;
  datag[1] = 0xFD;
  tx_load_seatalk_msg(0x86, 0, datag, 2);
}

void ST_send_PORTTACK()
{
  unsigned char datag[2];
  datag[0] = 0x21;
  datag[1] = 0xDE;
  tx_load_seatalk_msg(0x86, 0, datag, 2);
}

void ST_send_STBTACK()
{
  unsigned char datag[2];
  datag[0] = 0x22;
  datag[1] = 0xDD;
  tx_load_seatalk_msg(0x86, 0, datag, 2);
}

void ST_send_WINDMODE()
{
  unsigned char datag[2];
  datag[0] = 0x23;
  datag[1] = 0xDC;
  tx_load_seatalk_msg(0x86, 0, datag, 2);
}

void ST_send_boat_speed(float knots)
{
  unsigned int knots_10 = knots * 10;
  unsigned char datag[2];
  datag[0] = (knots_10 & 0x00FFU);    //Least significant byte first
  datag[1] = (knots_10 & 0xFF00U) >>  8;
  tx_load_seatalk_msg(0x20, 0, datag, 2);
}

void ST_send_awa(float awa)
{
  //10  01  XX  YY  Apparent Wind Angle: XXYY/2 degrees right of bow
  unsigned int awahex = (awa *2);  
  unsigned char datag[2];
  datag[0] = (awahex & 0xFF00U) >>8;    //split the value into byte1
  datag[1] = (awahex & 0x00FFU);        //and byte 2
  tx_load_seatalk_msg(0x10, 0, datag, 2);
}

void ST_send_aws(float aws)
{
 //11  01  XX  0Y  Apparent Wind Speed: (XX & 0x7F) + Y/10 Knots
  unsigned int awshex = aws;  //convert the float to int to remove decimal part
  unsigned char datag[2];
  datag[0] = (awshex & 0x00FFU) ;    //first datag is integer 
  aws = (aws - awshex)*10;          //subtract original value from int to be left with decimal part and *10
  datag[1] = aws;                   //2nd datatag = the deciaml part *10
  tx_load_seatalk_msg(0x11, 0, datag, 2);
}

///add more ST Send functions here...


void tx_seatalk_initialize()
{
  pinMode(Seatalk_TX_pin, OUTPUT);          //initialize: output
  digitalWrite(Seatalk_TX_pin, _12V_state);  //initialize: idle
  ST_line_state = _12V_state;
}

// parameters: CMD ID, attributes(without size), Data bytes array, Size of data array
void tx_load_seatalk_msg(unsigned char id, unsigned char attributes, unsigned char datarv[MAX_SIZE], unsigned char size_real)
{
  byte buffer_to_load = 0;
  boolean buff910 = false;
  while (buff910 == false)
  {
    for (int k3 = 0; k3 < tx_buffer_size; k3++) //check empty buffer
    {
      if (tx_buffer[k3].statuss == TX_EMPTY)
      {
        buffer_to_load = k3;
        buff910 = true;
        break;
      }
      delay(10);
    }
  }

  unsigned char att_byte = (attributes & 0x0F) <<  4;
  unsigned char size_part = (size_real & 0x0F) - 1;  //size excludes ID and attribute byte - 1
  att_byte = (att_byte | size_part);
  tx_buffer[buffer_to_load].sizee = size_real + 2;
  tx_buffer[buffer_to_load].msg[0] = id;
  tx_buffer[buffer_to_load].msg[1] = att_byte;


  String sm = "ST TX: " + String(id, HEX) + " " + String(att_byte, HEX);
  for (int b = 0; b < size_real; b++)
  {
    tx_buffer[buffer_to_load].msg[b + 2] = datarv[b];
    sm += " ";
    sm += String(datarv[b], HEX);
  }
  tx_buffer[buffer_to_load].statuss = TX_READY_TO_SEND; //mark ready

  if (tx_ST_debug)Serial.println(sm);
}



byte tx_tick_count = 0;

void ST_SOFT_TX_Intrrupt()
{
  tx_tick_count++;
  if (collision_detected > 3)  //SOFT RS takes 4 interrupt cycles to confirm state
  {
    collision_detected = 0;
    tx_tick_count = 0;
    ST_TX_sending = false;
    ST_idle_count = 0;
    digitalWrite(Seatalk_TX_pin, _12V_state);
    ST_line_state = _12V_state;
    anchor_bit = 0;
    anchor_byte = 0;
    if (tx_buffer[anchor_msg].statuss == TX_SENDING)
    {
      tx_buffer[anchor_msg].statuss = TX_READY_TO_SEND;
    }

    s += "#\n";
  }
  else if (tx_tick_count >= 8)
  {
    tx_tick_count = 0;

    if (tx_buffer[anchor_msg].statuss == TX_READY_TO_SEND)  //ready to send a byte
    {
      anchor_bit = 0;
      anchor_byte = 0;
      tx_buffer[anchor_msg].statuss = TX_SENDING;
      collision_detected = 0;
    }
    else if (tx_buffer[anchor_msg].statuss == TX_SENDING)  //start sending
    {
      if (anchor_bit == 0)  //start bit
      {
        /*
          if (tx_ST_debug)
          {
          s += String(anchor_msg) + "." + String(anchor_byte) + " " + String(tx_buffer[anchor_msg].msg[anchor_byte], HEX);
          s += "\tT ";
          }
        */
        if (ST_idle_count >= 50)
        {
          digitalWrite(Seatalk_TX_pin, _0V_state);  //start bit : 0V
          ST_line_state = _0V_state;
          anchor_bit = 1;
          ST_TX_sending = true;
          collision_detected = 0;
        }
      }
      else if ((anchor_bit > 0) && (anchor_bit <= 8)) //data bits
      {
        unsigned char bytt = tx_buffer[anchor_msg].msg[anchor_byte];
        boolean bitr =  bitRead(bytt, anchor_bit - 1);

        //if (tx_ST_debug)
        //   s += String(bitr);

        if (bitr)
        {
          digitalWrite(Seatalk_TX_pin, _12V_state);
          ST_line_state = _12V_state;
        }
        else
        {
          digitalWrite(Seatalk_TX_pin, _0V_state);
          ST_line_state = _0V_state;
        }
        anchor_bit++;
      }
      else if (anchor_bit == 9)
      {
        if (anchor_byte == 0) //cmd byte
        {
          //  if (tx_ST_debug)
          //   s += "C";

          digitalWrite(Seatalk_TX_pin, _12V_state);       //cmd bit on
          ST_line_state = _12V_state;
        }
        else
        {
          digitalWrite(Seatalk_TX_pin, _0V_state);        //cmd bit off
          ST_line_state = _0V_state;
        }
        anchor_bit = 10;
      }
      else if (anchor_bit == 10)
      {
        // if (tx_ST_debug)
        //  s += " P\n";

        digitalWrite(Seatalk_TX_pin, _12V_state);         //stop bit
        ST_line_state = _12V_state;
        anchor_bit = 11;
        if (anchor_byte >= tx_buffer[anchor_msg].sizee - 1) //last byte
        {
          for (int k4 = 0; k4 < MAX_SIZE; k4++) //emptying buffer
          {
            tx_buffer[anchor_msg].msg[k4] = 0x00;
          }
          tx_buffer[anchor_msg].statuss = TX_EMPTY; //mark done
          anchor_bit = 0;
          anchor_byte = 0;
        }
        else
        {
          tx_buffer[anchor_msg].statuss = TX_SENDING; //back to start bit
          anchor_bit = 0;
          anchor_byte++;
        }
      }
    }
    else
    {
      ST_TX_sending = false;
      anchor_msg++;
      if (anchor_msg > tx_buffer_size)
      {
        anchor_msg = 0;
      }
    }
  }
}
