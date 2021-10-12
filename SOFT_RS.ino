//RS states
#define RS_HAVE_NOTHING 0
#define RS_WAIT_HALF_A_BIT 1
#define RS_HAVE_STARTBIT 2
#define RS_PARITY 10
#define RS_WAIT_FOR_STOP 11
#define RS_HAVE_BYTE 12

//Bus states
#define BS_WAITING_FOR_COMMAND 0
#define BS_WAITING_FOR_SIZE 1
#define BS_RECEIVING_MESSAGE 2


unsigned char receive_tick_count = 0;
unsigned char last_message_character_position_written = 0;
unsigned char wait_for_command = true;
unsigned char parity;
unsigned char receive_state = RS_HAVE_NOTHING;
unsigned char byte_to_receive;
unsigned char bus_state = BS_WAITING_FOR_COMMAND;
unsigned char next_message = 0;
int expected_size = 255;


void seatalk_initialize()
{
  pinMode(Seatalk_RX_pin, INPUT);
  if (Seatalk_TX_enable)
  {
    tx_seatalk_initialize();
  }

  for (int p = 0; p < MAX_MSGS; p++)
  {
    seatalkbuff[p].statuss = MS_DONE;
    for (int yo = 0; yo < MAX_SIZE; yo++)
    {
      seatalkbuff[p].msg[yo] = 0;;
    }
  }

  Timer1.initialize(26);
  Timer1.attachInterrupt( timer3ISR );
}


void timer3ISR()
{
  boolean inSeatalk_RX_pin = digitalRead(Seatalk_RX_pin);
  
  if ( (inSeatalk_RX_pin == 0) && (ST_TX_sending == false)) //idle state
  {
    ST_idle_count++;
    if (ST_idle_count > 250)
    {
      ST_idle_count = 50;
    }
  }
  else if ( (inSeatalk_RX_pin == 1) && (ST_TX_sending == false)) //not idle, and not sent by this unit
  {
    ST_idle_count = 0;
  }
  else if ( (inSeatalk_RX_pin == 0) && (ST_TX_sending == true)) //collision check - _12V_state
  {
    if(ST_line_state==_0V_state)
    {
      collision_detected++;
    }
  }
  else if ( (inSeatalk_RX_pin == 1) && (ST_TX_sending == true)) //collision check - _0V_state
  {
    if(ST_line_state==_12V_state)
    {
      collision_detected++;
    }
  }

  

  input_rx(inSeatalk_RX_pin);

  if (Seatalk_TX_enable)
  {
    ST_SOFT_TX_Intrrupt();
  }
}


void input_rx(boolean SEATALK_DATA_READ)
{
  switch (receive_state)
  {
    case RS_HAVE_NOTHING:
      if (SEATALK_DATA_READ == 1)
      {
        receive_state = RS_WAIT_HALF_A_BIT;
        receive_tick_count = 0;
      }
      break;
    case RS_WAIT_HALF_A_BIT:
      receive_tick_count++;
      if (receive_tick_count == 4)
      {
        if (SEATALK_DATA_READ == 1)
        {
          receive_tick_count = 0;
          receive_state = RS_HAVE_STARTBIT;
          byte_to_receive = 0;
        }
        else
        {
          receive_state = RS_HAVE_NOTHING;
        }
      }
      break;

    default:
      receive_tick_count++;
      if (receive_tick_count == 8)
      {
        receive_tick_count = 0;
        receive_state++;
        byte_to_receive >>= 1;
        if (SEATALK_DATA_READ == 0)
        {
          byte_to_receive |= 0x80;
        }
      }
      break;

    case RS_PARITY:
      receive_tick_count++;
      if (receive_tick_count == 8)
      {

        receive_tick_count = 0;
        receive_state++;
        if (SEATALK_DATA_READ == 0)
        {
          parity = 1;
        }
        else
        {
          parity = 0;
        }
      }
      break;

    case RS_WAIT_FOR_STOP:
      receive_tick_count++;
      if (receive_tick_count == 8)
      {
        switch (bus_state)
        {
          case BS_WAITING_FOR_COMMAND:
            if (parity)
            {
              /* move on to next message*/
              next_message++;
              if (next_message == MAX_MSGS)
              {
                next_message = 0;
              }

              /* if next message is being read, move on again  */
              if (seatalkbuff[next_message].statuss == MS_READING)
              {
                next_message++;
                if (next_message == MAX_MSGS)
                {
                  next_message = 0;
                }
              }
              // s += '\n';
              //s += String(byte_to_receive, HEX);


              // s += ' ';
              seatalkbuff[next_message].msg[0] = byte_to_receive;
              bus_state = BS_WAITING_FOR_SIZE;
            }
            break;

          case BS_WAITING_FOR_SIZE:
            if (parity)
            {
              /* got an unexpected command message so must be a collision, abandon this message*/
              bus_state = BS_WAITING_FOR_COMMAND;
            }
            else
            {
              //s += String(byte_to_receive, HEX);
              // s += ' ';
              seatalkbuff[next_message].msg[1] = byte_to_receive;
              last_message_character_position_written = 1;
              bus_state = BS_RECEIVING_MESSAGE;
              expected_size = (byte_to_receive & 0x0f) + 2;
            }
            break;

          case BS_RECEIVING_MESSAGE:
            if (parity)
            {
              /* got an unexpected command message so must be a collision, abandon this message*/
              bus_state = BS_WAITING_FOR_COMMAND;
            }
            else
            {
              last_message_character_position_written++;
              if (last_message_character_position_written == MAX_SIZE)
              {
                bus_state = BS_WAITING_FOR_COMMAND;
              }
              else
              {
                seatalkbuff[next_message].msg[last_message_character_position_written] = byte_to_receive;
                //s += String(byte_to_receive, HEX);
                // s += ' ';

                if (last_message_character_position_written >= expected_size)
                {
                  /* mark previous message as ready */
                  seatalkbuff[next_message].sizee = expected_size;
                  expected_size = 255;
                  seatalkbuff[next_message].statuss = MS_READY;
                  bus_state = BS_WAITING_FOR_COMMAND;
                  seatalk_new_msg = true;
                }
              }
            }
            break;
        }
        receive_state = RS_HAVE_NOTHING;
      }
      break;
  }
}
