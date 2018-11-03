#include <Arduino.h>
#include "Dmx.h"

#define dmxMaxChannel  512
#define defaultMax 32

#define DMXSPEED       250000
#define DMXFORMAT      SERIAL_8N2
#define BREAKSPEED     83333
#define BREAKFORMAT    SERIAL_8N1

bool dmxStarted = false;
int sendPin = 1;	
uint8_t dmxData[dmxMaxChannel] = {};
int chanSize;


void DMXDev::init() {
  chanSize = defaultMax;

  Serial.begin(DMXSPEED);
  pinMode(sendPin, OUTPUT);
  dmxStarted = true;
}

// Set up the DMX-Protocol
void DMXDev::init(int chanQuant) {

  if (chanQuant > dmxMaxChannel || chanQuant <= 0) {
    chanQuant = defaultMax;
  }

  chanSize = chanQuant;

  Serial.begin(DMXSPEED);
  pinMode(sendPin, OUTPUT);
  dmxStarted = true;
}

// Function to read DMX data
uint8_t DMXDev::read(int Channel) {
  if (dmxStarted == false) init();

  if (Channel < 1) Channel = 1;
  if (Channel > dmxMaxChannel) Channel = dmxMaxChannel;
  return(dmxData[Channel]);
}

// Function to send DMX data
void DMXDev::write(int Channel, uint8_t value) {
  if (dmxStarted == false) init();

  if (Channel < 1) Channel = 1;
  if (Channel > chanSize) Channel = chanSize;
  if (value < 0) value = 0;
  if (value > 255) value = 255;

  dmxData[Channel] = value;
}

void DMXDev::end() {
  delete dmxData;
  chanSize = 0;
  Serial.end();
  dmxStarted == false;
}

void DMXDev::update() {
  if (dmxStarted == false) init();

  //Send break
  digitalWrite(sendPin, HIGH);
  Serial.begin(BREAKSPEED, BREAKFORMAT);
  Serial.write(0);
  Serial.flush();
  delay(1);
  Serial.end();

  //send data
  Serial.begin(DMXSPEED, DMXFORMAT);
  digitalWrite(sendPin, LOW);
  Serial.write(dmxData, chanSize);
  Serial.flush();
  delay(1);
  Serial.end();
}
