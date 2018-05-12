#include <nRF24L01.h>
#include <printf.h>
#include <RF24.h>
#include <RF24_config.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>

long xAccelRaw, yAccelRaw, zAccelRaw;
float xAccelNormal, yAccelNormal, zAccelNormal;
int xIsLeftRight, yIsForBack;
int stopped = 0;
#define CE 12
#define CS 6
#define light 8
#define flexPin A9


Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, light, NEO_GRB + NEO_KHZ800); Flora oboard neopixel
RF24 radio(CE,CS);

byte addresses[][6] = {"Node1", "Node2"};


void setup()
{

  Wire.begin();
  Serial.begin(9600);
  radio.begin();
  strip.begin();

  strip.setBrightness(150);
  strip.show();
  
  radio.setPALevel(RF24_PA_LOW);
  radio.setPayloadSize(2);

  radio.openWritingPipe(addresses[1]);
  radio.openReadingPipe(1, addresses[0]);

  radio.maskIRQ(1,1,0); //Turn on interrupt only when there is a package received
  attachInterrupt(2, interruptFunction, FALLING); //Pin #0 (RX) used to detect interrupt
  
  //Let's set up the MPU to wake up
  Wire.beginTransmission(0b1101000); //Slave address. LSB 0 because AD0 pulled to ground.
  Wire.write(107); //Access Power Management register because device automatically comes in sleep mode
  Wire.write(0b00000000); //Turn off sleep mode and sets internal oscillator as clock source.
  Wire.endTransmission();

  //Configure sensitivity of Gyroscope
  Wire.beginTransmission(0b1101000); //Slave address
  Wire.write(27); //Gyrscope configuration register
  Wire.write(0b00000000); //Range of +- 250 degrees per second.
  Wire.endTransmission();

  //Configure sensitivity for Accelerometer
  Wire.beginTransmission(0b1101000); //Slave address
  Wire.write(28); //Accelerometer configuration register
  Wire.write(0b00000000); //Range of +-2g
  Wire.endTransmission();
  
  radio.stopListening(); //Writing mode


}

void loop()
{
  bool honk = false;
  if(analogRead(flexPin)< 80) //Is sensor being flexed?
  {
    honk = true;
  }
  
  getAcceleration();
  normalizeValues();
  xIsLeftRight = (int) (xAccelNormal * 250); //Map into a motor shield speed (0-250)
  yIsForBack = (int) (yAccelNormal * 250);
  sendDirection(honk); //Encode and transmit info


  radio.startListening();
  delay(5);
  radio.stopListening();

  

}

void interruptFunction()
{
  int stopped;
  while(radio.available())
  {
    radio.read(&stopped, sizeof(stopped));
  }
  if(stopped == 1) //If obstacle is encountered, turn on onboard light.
  {
     strip.setPixelColor(0, 255, 90, 80);
     strip.show();
     delay(1000);
     strip.setPixelColor(0, 0, 0 ,0);
     strip.show();
  }
}


/* Encoding 3 things into 1 int value (Decoding will take place in receiver):
   General Direction (x or y)
   Specific direction (left right, or forward backward)
   Should it honk?
*/
void sendDirection(bool toHonk)
{
  int toSend;
  if(!toHonk) //No honk
  {
   if(abs(xIsLeftRight)>60) //If left right tilt significant enough
   {
      if(xIsLeftRight<0) //Left (neg)
      {
        toSend = xIsLeftRight - 1000; //The 1000 is to differentiate from the a y (forward backward) value, since
                                      //x and y can only go up to 250 (the 1000s are unclaimed).
      }
      else //Right (pos)
      {
        toSend = xIsLeftRight + 1000;
      }

     radio.write(&toSend, sizeof(toSend));
   }
   else //Sending forward backward info
    {
      radio.write(&yIsForBack, sizeof(yIsForBack));
    }
  }
  else //Honk
  {
   if(abs(xIsLeftRight)>60)
   {
      if(xIsLeftRight<0) //Send left right with honk
      {
        toSend = xIsLeftRight - 1000 - 10000; //The 10,000 is to differentiate no honk with honk since the 1000s are already
                                              //being used to differentiate x from y.
      }
      else
      {
        toSend = xIsLeftRight + 1000 + 10000;
      }

     radio.write(&toSend, sizeof(toSend));
   }
   else //Send forward backward with honk
    {
      if(yIsForBack < 0)
      {
        toSend = yIsForBack - 10000;
      }
      else
      {
        toSend = yIsForBack + 10000;
      }
      radio.write(&toSend, sizeof(toSend));
    }
  }
}

void getAcceleration()
{
  Wire.beginTransmission(0b1101000); //Begin I2C communication
  Wire.write(59); //Begin communication with accelerometer values register
  Wire.endTransmission();
  Wire.requestFrom(0b1101000, 6); // Requesting accelerometer data from registers 59-64
  while(Wire.available() < 6); //Wait until all 6 registers have transmitted the data
  xAccelRaw = Wire.read() <<8 | Wire.read(); //Put x axis accelerometer value back together (16 bits)
  yAccelRaw = Wire.read() <<8 | Wire.read();// Put y accelerometer value back together
  zAccelRaw = Wire.read() <<8 | Wire.read(); //Z accelerometer value
}

void normalizeValues()
{
  xAccelNormal = (xAccelRaw / 16384.0); //Converts it to G's. The divisor depends on the range of the measurements selected (+-2g)
  yAccelNormal = (yAccelRaw / 16384.0);
  zAccelNormal = (zAccelRaw / 16384.0);
}
