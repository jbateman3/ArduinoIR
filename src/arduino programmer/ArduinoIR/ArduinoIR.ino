/*
 * Acts as a intermediate programmer for the AvrDudeIR software 
 * and the AvrDudeIR bootloader on the target
 * 
 * The main Task is to forward serial data recived from the host
 * over to the target by IR to program it
 * 
 * TODO - switch IRsendBootloaderCMD to use the official GMC IR protocal
 * 
 */

#include "SoftwareSerialIR.h"

//Create a software Serial port tailared to IR
SoftwareSerialIR mySerialIR(9); // RX, TX, IR Period

// Buffers the data recived from the computer as it
// is trnsfered faster than it is send out
// only buffers a page at a time
unsigned char pageBuffer[128];
// Index of where in the buffer has been sent out
unsigned char bufferReadIndex = 0;
// Index in the buffer that has been written to
unsigned char bufferWriteIndex = 0;
// The index in the buffer to request the next data
unsigned char bufferRequestDataIndex = 90;

// 1 second timeout for waitng for data from the host
unsigned long reciveTimeout = 1000;
unsigned long lastMillis = 0;

void setup() {

  // Serial link to the host
  // negautated later to be the same as the target
  Serial.begin(4800);
  
}

void loop() { 

// Wait for data from the host to start programming
if (Serial.available() > 0){
  //First 3 byte from the host is the period for the IR carrior, buad rate and pairt bit settings
  //When the target is using software serial the baud rate is fixed to 4800
  uint8_t setupData[3];
  Serial.readBytesUntil(0,setupData,3);
  unsigned char timerPeriod = setupData[0];
  // Read in the baud rate to proram with
  unsigned char baudRateHigh = setupData[1];
  unsigned char baudRateLow = setupData[2];
  // Highest bit of baud rate is reserved to indicate if parity bits should be used
  unsigned char parityBit = baudRateHigh >> 7;
  unsigned int baudRate = (((unsigned int)baudRateHigh & 0b01111111) << 8) | (unsigned int)(baudRateLow);
  //Set and enable the IR carrior wave
  //Defaut sreial is 4800 and parity bit, negashated later
  mySerialIR.begin(4800, timerPeriod,1);//mySerialIR.SetIRPeriod(timerPeriod);//FrequencyTimer2::setPeriod(timerPeriod);
  

   // Cutom reset command to reboot the target into the booloader
   //Target program must implement this to have any effect
  IRsendBootloaderCMD();
  
  //Send the value back to the host to indicate we are ready
  // and the comminication is working
  Serial.write(timerPeriod);
  Serial.write(baudRate>>8);
  Serial.write(baudRate);
  Serial.write(parityBit);
  Serial.flush();
  // Change the baud rate to be the same as the connection to the target
  Serial.end();
  Serial.begin(baudRate);
  delay(2);  
  
    //Make sure at least 5 0xaa have been sent before uploading settings
    for(int x = 0; x < 10; x++){
      mySerialIR.write(0xaa);
      delay(2);
    }

    //When using software serial on the target the baud rate is fixed
    ///////////////
    //Send configuration data to the device to configure UART
    //mySerialIR.write(':');
    //mySerialIR.write(setupData[1]);
    //mySerialIR.write(setupData[2]);

    // Reconfigure SoftwareSerialIR to new settings
    //mySerialIR.end();
    //mySerialIR.begin(baudRate, timerPeriod,parityBit);

    //delay(2);

    //Make sure at least 5 0xaa have been sent before uploading code
    //for(int x = 0; x < 6; x++)
    //  mySerialIR.write(0xaa);
    ///////////////
   

     //Wait for the target to be reset and the host to send the start singnal
  while(Serial.available() == 0){
    //Target not read send 0xaa to keep it in bootloader
      mySerialIR.write(0xaa);
      delay(2);
  }

  // Chec the recived byte is indicating to start programming
  unsigned char startSig = Serial.read();
  if (startSig == 's'){


  // Make sure buffer indexes are reset
  bufferReadIndex = 0;
  bufferWriteIndex = 0;
  bufferRequestDataIndex = 90;

  //Count the bytes so we know where we are in the recived data
  int recivingState = 1;
  int dataToRecive = 0;

  //Read all the data from the host
  //Break when all data is read on timeout
  lastMillis = millis();
  for(;;){
    //Check if the host has sent us data, store it in the buffer
    while (Serial.available() > 0){
      pageBuffer[bufferWriteIndex] = Serial.read();

      // Increment looping buffer
       bufferWriteIndex++;     
      if ( bufferWriteIndex >= 120)
        bufferWriteIndex = 0;
    }

    // Check if we have recived more data thab we have sent out
    if (bufferWriteIndex != bufferReadIndex){
      // Forward the recived byte to the target
      mySerialIR.write(pageBuffer[bufferReadIndex]);

      //Update the reciving state - this keeps track of how far thoug the data we have progressed
      // all to send a delay at the end of a page...
      if (recivingState == 1){
        recivingState++; // First byte ';' - start of command
      }else if (recivingState == 2){
        dataToRecive = pageBuffer[bufferReadIndex]-1; // Second byte - data length
        recivingState++;
      }else if (recivingState == 3 || recivingState == 4 || recivingState == 5){
        recivingState++; // Dont care about nex t three bytes
      }else if (dataToRecive != 0){
        dataToRecive--; // Dont care about the data bytes
      }else if (dataToRecive == 0){
        recivingState = 1; // End of page - delay to allow the flash to program
        delay(15);
      }
     // Check if we have recived 90 bytes
     // Send 'N' to request the next 100 bytes
     if (bufferRequestDataIndex == bufferReadIndex){
        Serial.write(0x4E);
        bufferRequestDataIndex = bufferReadIndex + 101;
        if (bufferRequestDataIndex >= 120)
          bufferRequestDataIndex -= 120;
     }      
      bufferReadIndex++;
      //Update millis for the timeout
     lastMillis = millis();
     // Increment looping buffer
     if ( bufferReadIndex >= 120){
      bufferReadIndex=0;
     }
    }
    //Check if we have reviced data in the 1 seconds
    if (millis() - lastMillis > reciveTimeout){
      break;
    }
  }
}
// Finished communicateing end connection so port can be reconfigured
mySerialIR.end();
  Serial.end();
  Serial.begin(4800);
}
}

// Turns timer 2 on or off
// turns the carrior wave on or off
void toggleTimer() {
  static int timerState = 0;
  if (timerState == 0) {
    timerState = 1;
    FrequencyTimer2::enable();
  } else {
    timerState = 0;
    FrequencyTimer2::disable();
  }
}


///////////////////////////////////////////////////////////////////////////////
/////////////// IR Encoding from MY GMC to send bootloader cmd ////////////////
///////////////////////////////////////////////////////////////////////////////

bool IRsendBootloaderCMD() {
 // int txPinState = digitalRead(10);
 // digitalWrite(10,HIGH);
  
  static long lastTransmit = 0;
  for(int SendCount = 0; SendCount < 3; SendCount++){
     // Special bit sequence to enter bootloader
     // Needs to be sent 3 times
    long dataToSend = 0b01010101010101010101010101010101;
    for (int x = 0; x < 31; x++) {
      toggleTimer();
      if (bitRead(dataToSend, x) == 1) {
        delayMicroseconds(600);
      } else {
        delayMicroseconds(300);
      }
    }
    toggleTimer();
    if(SendCount != 2)
    delayMicroseconds(15000);
  }
//  digitalWrite(10,txPinState);
}
