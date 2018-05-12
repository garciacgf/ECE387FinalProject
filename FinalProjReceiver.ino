#include <Adafruit_MotorShield.h>
#include <Wire.h>

#include <nRF24L01.h>
#include <printf.h>
#include <RF24.h>
#include <RF24_config.h>

int received, stopped;
long duration;

#define CE 7
#define CS 8

#define redLights 4

#define trig 5
#define echo 6

#define honk 9

RF24 radio(CE, CS);
Adafruit_MotorShield AFMS = Adafruit_MotorShield();

Adafruit_DCMotor *left = AFMS.getMotor(1);
Adafruit_DCMotor *right = AFMS.getMotor(2);

byte addresses[][6] = {"Node1", "Node2"};

void setup()
{
  Serial.begin(9600);
  radio.begin();
  AFMS.begin();

  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  pinMode(honk, OUTPUT);
  pinMode(redLights, OUTPUT);

  radio.setPALevel(RF24_PA_LOW);
  radio.setPayloadSize(2);

  radio.openWritingPipe(addresses[0]);
  radio.openReadingPipe(1, addresses[1]);

  radio.startListening();
}

void loop()
{
  stopped = 0;
  if(readUltraSonic() < 10)
  {
    stopped = 1;
  }

  radio.stopListening();
  radio.write(&stopped, sizeof(stopped));

  radio.startListening();

  if(radio.available())
  {
       Serial.println("Hello");

    while(radio.available())
    {
      radio.read(&received, sizeof(received));
    }

    decodeMessage();
  }

  
}

int readUltraSonic()
{
    //Clearing trig pin
  digitalWrite(trig, LOW);
  delayMicroseconds(2);

  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  duration = pulseIn(echo, HIGH);

  int distance = duration*.034/2;
  return distance;
}

void decodeMessage()
{
  bool toHonk = false;
  int x, y;
  if(abs(received) >= 10000) //Honk
  {
    toHonk = true;
    if(abs(received) >= 11000) //LeftorRight
    {
      if(received < 0) //Left
      {
        x = received + 1000 + 10000;
      }
      else //Right
      {
        x = received - 1000 - 10000;
      }
    }
    else //Forward or Backward
    {
      if(received < 0) // Backward
      {
        y = received + 10000;
      }
      else //Forward
      {
        received - 10000;
      }
    }
  }
  else //No honk
  {
    if(abs(received) >= 1000) // Left or right
    {
      if(received < 0) //Left
      {
        x = received + 1000;
      }
      else // Right
      {
        x = received - 1000;
      }
    }
    else //Forward or Backward
    {
      y = received;
    }
  }

  doSomething(x, y, toHonk);
}

void doSomething(int x, int y, bool honking)
{
  int leftPwr, rightPwr;

      left->run(RELEASE);
    right->run(RELEASE);
  if(honking)
  {
    digitalWrite(honk, HIGH);
  }

  if(abs(x) > 0 && stopped == 0)
  {
    if(x < 0) //Left
    {
      leftPwr = abs(x)/4;
      rightPwr = abs(x);
    }
    else //Right
    {
      leftPwr = x;
      rightPwr = x/4;
    }

    left->setSpeed(leftPwr);
    right->setSpeed(rightPwr);
    left->run(FORWARD);
    right->run(FORWARD);
  }
  else if(abs(y) > 15 && stopped == 0)
  {
    if(y < 0) //Backward
    {
      leftPwr = abs(y);
      rightPwr = abs(y);

      left->setSpeed(leftPwr);
      right->setSpeed(rightPwr);
      left->run(BACKWARD);
      right->run(BACKWARD);

      digitalWrite(redLights, HIGH);

      Serial.println("Backwards");
    }
    else //Forward
    {
      leftPwr = y;
      rightPwr = y;

      left->setSpeed(leftPwr);
      right->setSpeed(rightPwr);
      left->run(FORWARD);
      right->run(FORWARD);

      Serial.println("Forward");
    }
  }
  else // Neither of them significant enough or obstacle detected
  {
    left->run(RELEASE);
    right->run(RELEASE);
  }

  delay(100);

  digitalWrite(honk, LOW);
  digitalWrite(redLights, LOW);
}

