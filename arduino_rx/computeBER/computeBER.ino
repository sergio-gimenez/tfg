#include <IRremote.h>
#include <Wire.h>


// Timeout for the BER computation in order to know the last message
#define timeout 5000


// IR VARIABLES \\

int RECV_PIN = 11;

IRrecv irrecv(RECV_PIN);

decode_results results;

// Expected message from the sender (generated randomly)
long expected_message = 0b00000000000000000000000000000000;

// Acumulative sum of the wrong bits of the transmission
long wrong_bits;

// Total number of messages received
int tx_count;

// Flag to know when the transmission starts
boolean tx_started = false;

// Timestamp for the last received message
int last_message_timestamp = 0;


// I2C variables \\

int i2c_bytes_count;

unsigned long rbuf[5];

int rx_checksum, computed_checksum;

unsigned long msg;

boolean isEOT = false;

boolean isChecksumComing = false;

int offset;

long rcv_i2c_pkts;

boolean isI2CinBuf = false;

boolean isIRinBuf = false;



long get_wrong_bits(long expected_msg, long received_msg);



void setup()
{
  Serial.begin(9600);

  // In case the interrupt driver crashes on setup, give a clue
  // to the user what's going on.
  irrecv.enableIRIn();           // Start the receiver
  Wire.begin(8);                // join i2c bus with address #8
  Wire.onReceive(receiveEvent); // register event
  Serial.println("Enabled IRin and I2C bus");

}

void loop() {

  if (irrecv.decode(&results) && isI2CinBuf) {
    Serial.println("i2c packet is in buffer");
    tx_started = true;
    Serial.print("IR Received message: ");
    Serial.println(results.value, HEX);
    Serial.println("\n");

    tx_count++;

    // Sum of all wrong bits in the transmission
    wrong_bits += get_wrong_bits(results.value, expected_message);

    // Start the timeout
    last_message_timestamp = millis();

    // Receive the next value
    irrecv.resume();

    //isIRinBuf = true;
    isI2CinBuf = false;

  }

  // Check if the timeout is over
  unsigned int time_elapsed = millis() - last_message_timestamp;
  if (((time_elapsed > timeout) && tx_started)) {
    double normalize = 32 * tx_count;
    double BER = wrong_bits / normalize;
    Serial.print("Average BER for the transmission: ");
    Serial.println(BER, 20);
    Serial.print("Received i2c pckts: ");
    Serial.println(rcv_i2c_pkts / 5);
    Serial.print("Total time elapsed: ");
    Serial.println(millis()/1000 - 5);

    tx_started = false;
    time_elapsed = 0;
    tx_count = 0;
    wrong_bits = 0;
  }
  delay(100);
}


// Detect the errors in the message received and returns the amount of errors in a message.
// Argumnents:
// - input_message: The received message you actually want to count the errors.
// - expected_message: The expected received message
long get_wrong_bits(long expected_msg, long received_msg)
{
  long count = 0;

  // The messages are 32 bits max.
  for (int i = 0; i < 32; i++) {

    // right shift both the numbers by 'i' and
    // check if the bit at the 0th position is different
    if (((expected_msg >> i) & 1) != ((received_msg >> i) & 1)) {
      count++;
    }
  }
  return count;
}


// Function that executes whenever data is received from master
// This function is registered as an event, see setup()
void receiveEvent(int howMany) {

  rcv_i2c_pkts++;

  // Received int
  rbuf[i2c_bytes_count] = Wire.read();

  // Count for the first 5 bytes since packet length is 5 (msg + cheksum)
  if (i2c_bytes_count < 5) {
    if (!isChecksumComing) {
      msg |= rbuf[offset / 8] << offset;
    }

    if (i2c_bytes_count == 3) {
      isChecksumComing = true;
    }

    if (isChecksumComing && i2c_bytes_count == 4) {
      rx_checksum = rbuf[i2c_bytes_count];
      isEOT = true;
    }

    i2c_bytes_count ++;
    offset += 8;
  }

  if (isEOT) {

    // Verify that received message is not a i2c scan
    if (!(msg == 0xFFFFFFFF)) {

      Serial.print("I2C Received message = ");
      Serial.println(msg, HEX);

      for (int i = 0; i < 4; i++) {
        computed_checksum += rbuf[i];
      }
      computed_checksum = computed_checksum & 0xff;

      if (computed_checksum == rx_checksum) {
        expected_message = msg;
      }
    }

    // End of i2c transmission. Reinitialize variables    
    msg = 0;
    i2c_bytes_count = 0;
    offset = 0;
    isEOT = false;
    isChecksumComing = false;
    computed_checksum = 0;
    isI2CinBuf = true;

  }
}
