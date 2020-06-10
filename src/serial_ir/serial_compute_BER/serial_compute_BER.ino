#include <Wire.h> // I2C library

#define LAST_IR_MESSAGE_TIMEOUT 5000
#define PKT_LENGTH 4 // In bytes
#define I2C_BUS_ADDRESS 8
#define ACK 0x06 //ACK character 


long global_timer_start;

// IR VARIABLES \\

// Expected message from the sender (generated randomly)
long expected_message = 0b00000000000000000000000000000000;
unsigned long wrong_bits_sum;
unsigned long received_msgs_count;
boolean has_tx_started = false;
long last_message_timestamp = 0;
long ir_rcv_msg;

// I2C variables \\

int i2c_bytes_count;
unsigned long i2c_rbuf[PKT_LENGTH];
unsigned long msg;
boolean isEOT = false;
int offset;
unsigned long rcv_i2c_pkts;
boolean isI2CinBuf = false;

void setup()
{
  Serial.begin(2400);

  // join i2c bus
  Wire.begin(I2C_BUS_ADDRESS);

  // register event
  Wire.onReceive(handle_i2c_event);

  // In case the interrupt driver crashes on setup, give a clue to the user what's going on.
  Serial.println("Arduino is ready\n");
}

void loop() {
  
  // Get IR message only if an I2C pkt has been received
  if (isI2CinBuf) {
    
    if(!has_tx_started){
      global_timer_start = millis();
    }    
    has_tx_started = true;

    while (ir_rcv_msg == 0) {
      ir_rcv_msg = Serial.parseInt();
    }
    
    //Serial.print(" IR Received message = ");
    //Serial.println(ir_rcv_msg, HEX);
    //Serial.println("\n");

    received_msgs_count++;

    // Sum of all wrong bits in the transmission
    wrong_bits_sum += get_wrong_bits(expected_message, ir_rcv_msg);

    last_message_timestamp = millis();

    isI2CinBuf = false;
    ir_rcv_msg = 0;

    Serial.write(0x06);    
  }

  

  // Check if the timeout is over
  unsigned int time_elapsed = millis() - last_message_timestamp;
  if (((time_elapsed > LAST_IR_MESSAGE_TIMEOUT) && has_tx_started)) {

    double BER = (double) wrong_bits_sum / (double)(32 * received_msgs_count);

    //TODO wrap all the prints in a function

    Serial.println("\n\nEXPERIMENT REPORT");
    Serial.println("-------------------\n");

    Serial.print("\n- Total wrong bits in the transmission: ");
    Serial.println(wrong_bits_sum);

    Serial.print("\n- Total bits received: ");
    long total_bits_received =  received_msgs_count * 32;
    Serial.println(total_bits_received);

    Serial.print("\n- Average BER for the transmission: ");
    Serial.println(BER, 20);
    Serial.println("    * Disclaimer: This BER value might be wrong, arduino doesn't deal good with big decimal numbers. ");
    Serial.println("      Anyhow the BER value is just: BER = total_wrong_bits / total_bits_received;");


    Serial.print("\n- Total time elapsed: ");
    Serial.print((last_message_timestamp - global_timer_start) / 1000);
    Serial.println(" seconds");

    Serial.print("\n\nDuring the whole transmission, received ");
    Serial.print(rcv_i2c_pkts / PKT_LENGTH);
    Serial.print(" i2c pacekts and ");
    Serial.print(received_msgs_count);
    Serial.println(" IR packets\n\n");

    has_tx_started = false;
    time_elapsed = 0;
    received_msgs_count = 0;
    wrong_bits_sum = 0;
  }
}


// Detect the errors in the message received and returns the amount of errors in a message.
// Argumnents:
// - received_msg: The received message you actually want to count the errors.
// - expected_message: The expected received message
long get_wrong_bits(long expected_msg, long received_msg)
{
  long count = 0;

  // The messages are 32 bits max.
  for (int i = 0; i < 32; i++) {

    // right shift both the numbers by 'i' and
    // check if the bit at the ith position is different
    if (((expected_msg >> i) & 1) != ((received_msg >> i) & 1)) {
      count++;
    }
  }
  return count;
}


// Function that executes whenever data is received from master
// This function is registered as an event, see setup()
void handle_i2c_event() {

  rcv_i2c_pkts++;

  i2c_rbuf[i2c_bytes_count] = Wire.read();

  // TODO I could wrap the bit shifting in a function to make it more readable
  if (i2c_bytes_count < PKT_LENGTH) {

    msg |= i2c_rbuf[offset / 8] << offset;

    if (i2c_bytes_count == (PKT_LENGTH - 1)) {
      isEOT = true;
    }

    i2c_bytes_count ++;
    offset += 8;
  }

  if (isEOT) {
    // End of i2c transmission. Reinitialize variables
    msg = 0;
    i2c_bytes_count = 0;
    offset = 0;
    isEOT = false;
    isI2CinBuf = true;
  }
}
