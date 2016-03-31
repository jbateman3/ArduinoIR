

int pin = 3;

volatile int val1 = 0;
volatile int val2 = 0;
volatile int val3 = 0;
volatile int dataValid = 0;

int testPin = 7;


int ReciveCount = 0;
int ReciveCorrectCount = 0;

unsigned long timeCount = 1000;


unsigned long timeCount2 = 1000;
int ledSate = 0;


void (*boot_bootloader)(void) = (void (*)())0x3c00;

void pciSetup(byte pin)
{
    *digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin));  // enable pin
    PCIFR  |= bit (digitalPinToPCICRbit(pin)); // clear any outstanding interrupt
    PCICR  |= bit (digitalPinToPCICRbit(pin)); // enable interrupt for the group
}

void setup() {
  pinMode(testPin, OUTPUT);
  pinMode(pin, INPUT);
  pinMode(13, OUTPUT);

  pinMode(0, INPUT_PULLUP);
  pciSetup(0);
  
  //attachInterrupt(1, bitChange, CHANGE);
  
}

void loop() {
    int var = 150;
    for(int xyz = 0; xyz < 10; xyz++){
      digitalWrite(13, HIGH);
      delay(var);
      digitalWrite(13, LOW);
      delay(var);
    }
    delay(1000);    
}



ISR (PCINT2_vect) // handle pin change interrupt for D0 to D7 here
{
 static long bitTime = 0;
 static int recivingData = -1;
 static int bitIndex = 0;
 static long recivedData = 0;

static long bootloaderCmd = 0b01010101010101010101010101010101;
static short bootloaderCmdNum = 0;
 
  long changeTime = micros();
  long deltaTime = changeTime - bitTime;


  
  if (deltaTime >= 10000){
    recivingData = 0;
    dataValid = 0;
    bitIndex = 0;
  recivedData = 0;
  } else if (recivingData != -1){
    int valueRecived = -1;
   if ( deltaTime <= 450){ // Recived 0
     valueRecived = 0;
   }else if (deltaTime <= 1000){ // Recived 1
     valueRecived = 1;
   }else{
     recivingData = -1; // Invalid time, reset sequence
   }
   if (valueRecived != -1){
    if (valueRecived == 1)
      bitSet(recivedData, bitIndex);
    bitIndex++;
    if (bitIndex >= 31){
      recivingData = -1;
      if (recivedData == bootloaderCmd){
        bootloaderCmdNum++;
        if (bootloaderCmdNum == 3){
          bootloaderCmdNum = 0;
          *digitalPinToPCMSK(pin) = 0; // Disable Interrupt
          PORTD &= 0b01111111;PORTD |= 0b10000000;PORTD &= 0b01111111;PORTD |= 0b10000000;
          PORTD &= 0b01111111;PORTD |= 0b10000000;PORTD &= 0b01111111;PORTD |= 0b10000000;
          PORTD &= 0b01111111;PORTD |= 0b10000000;PORTD &= 0b01111111;PORTD |= 0b10000000;
          boot_bootloader();
        }
      }else{
        bootloaderCmdNum=0;
        validateData(recivedData);
        val1 = 0;
        val2 = 0;
        val3 = 0;
        val1 = (recivedData >> 1) & 0b111111111;
        val2 = (recivedData >> 10) & 0b111111;
        val2 |= (recivedData >> 11) & 0b111000000;
        val3 = (recivedData >> 20) & 0b1111;
        val3 |= (recivedData >> 21) & 0b1110000;
        val3 |= (recivedData >> 22) & 0b10000000;
        dataValid = 1;         
      }
    
    }
   }
  }
  bitTime = changeTime;
}

bool validateData(long &data){
  PORTD |= 0b10000000;
  static char parityBitIndexes[] = { 31, 30, 28, 24, 16, 0 };
  static char parityBitIndex[] = { 1, 2, 4, 8, 16, 32 };
  int parityBitCounts[] = { 0, 0, 0, 0, 0, 0 };
  int wrongParityBits[] = {0, 0, 0, 0, 0, 0 };
  int bitsToRead = 1;
  int bitStartPos = 31;
  int bitPosChange = -2;

  for (int parityBit = 0; parityBit < 5; parityBit++) {
    for (int bitPos = bitStartPos; bitPos >= 0; bitPos += bitPosChange) {
      for (int bitCountPos = bitPos; bitCountPos > bitPos - bitsToRead; bitCountPos -= 1) {
        if (bitRead(data, bitCountPos) == 1) parityBitCounts[parityBit]++;
      }
    }
    bitStartPos -= bitsToRead;
    bitPosChange *= 2;
    bitsToRead *= 2;
  }
  for (int parityBit = 0; parityBit < 5; parityBit++) {
    if (parityBitCounts[parityBit] % 2 == 1) {
      wrongParityBits[parityBit] = 1;
    }
  }
  int sum = 0;
    for (int parityBit = 0; parityBit < 5; parityBit++) {
    if (wrongParityBits[parityBit] == 1){
      sum += parityBitIndex[parityBit];
    }
  }
  if (sum != 0)
  bitWrite(data, 31-sum, !bitRead(data, 31-sum));
  PORTD &= 0b01111111;
}
