#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>
#include <stdio.h>

const int PORT = 5000;
const int SIZE = 8;
const int MOTOR1 = 3;
const int MOTOR2 = 5;
const int MOTOR3 = 6;
const int MOTOR4 = 9;
const int MOTOR5 = 10;

BridgeServer server(PORT);
  
void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(MOTOR1, OUTPUT);
  pinMode(MOTOR2, OUTPUT);
  pinMode(MOTOR3, OUTPUT);
  pinMode(MOTOR4, OUTPUT);
  pinMode(MOTOR5, OUTPUT);

  digitalWrite(LED_BUILTIN, 1);
  Bridge.begin();
  Console.begin();
  /*while (!Console) {
    ;
  }*/
  digitalWrite(LED_BUILTIN, 0);

  
  Console.println("Listenning for connection");
  server.noListenOnLocalhost();
  server.begin();
  
}

void loop() {
  // put your main code here, to run repeatedly:
  BridgeClient client = server.accept();

  if (client) {
    Console.println("A client connected");
    client.println("HELLO FROM ARDUINO");
    client.setTimeout(5);
    
    while (client.connected()) {
      //     
      if (client.available()) {

        unsigned int motor;
        unsigned int intensity;
        unsigned int op_time;

        ((byte*)&motor)[0] = client.read();
        ((byte*)&motor)[1] = client.read();

        ((byte*)&intensity)[0] = client.read();
        ((byte*)&intensity)[1] = client.read();

        ((byte*)&op_time)[0] = client.read();
        ((byte*)&op_time)[1] = client.read();

        switch(motor) {
          case 1: {
            analogWrite(MOTOR1, intensity);
            analogWrite(MOTOR2, 0);
            analogWrite(MOTOR3, 0);
            break;
          }
          case 2: {
            analogWrite(MOTOR2, intensity);
            analogWrite(MOTOR1, 0);
            analogWrite(MOTOR3, 0);            
            break;
          }
          case 3: {
            analogWrite(MOTOR3, intensity);
            analogWrite(MOTOR1, 0);
            analogWrite(MOTOR2, 0);            
            break;
          }
        }

        delay(op_time);
                
        Console.println(motor);
        Console.println(intensity);
        Console.println(op_time);

        analogWrite(MOTOR1, 0);
        analogWrite(MOTOR2, 0);
        analogWrite(MOTOR3, 0);
        uint8_t a = 2;
        client.write(a);
        client.flush();
        
      }
      
    }
    client.stop();
  }

  
}
