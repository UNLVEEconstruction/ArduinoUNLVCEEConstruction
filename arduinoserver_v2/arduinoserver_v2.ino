#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>
#include <stdio.h>

#define PORT (5000)
#define LED (LED_BUILTIN)
#define CLIENT_COUNT (2)
#define MOTOR_COUNT (3)
#define MOTOR_0_PIN (3)
#define MOTOR_1_PIN (5)
#define MOTOR_2_PIN (6)
#define WELCOME_MSG "HELLO FROM ARDUINO"
#define DENY_MSG "ARDUINO IS BUSY. GOODBYE"

struct node {
  int intensity;
  int duration;
  unsigned long start_time;
  node *next;
};

struct motor_control_info {
  uint8_t motor_status;
  node *head;
  node *tail;
};

struct client_info {
  bool is_free;
  BridgeClient client;
};

BridgeServer server(PORT);
client_info client[CLIENT_COUNT];
motor_control_info motor[MOTOR_COUNT];
int connection_count;

void setup() {
  // put your setup code here, to run once:
  pinMode(LED, OUTPUT);
  pinMode(MOTOR_0_PIN, OUTPUT);
  pinMode(MOTOR_1_PIN, OUTPUT);
  pinMode(MOTOR_2_PIN, OUTPUT);


  digitalWrite(LED , 1);
  Bridge.begin();
  Console.begin();
  /*while (!Console) {
    ;
  }*/
  digitalWrite(LED ,0);

  //
  for (int i=0; i<CLIENT_COUNT; i++) {
    client[i].is_free = true;
  }

  //
  for (int i=0; i<MOTOR_COUNT; i++) {
    motor[i].motor_status = 0;
    motor[i].head = motor[i].tail = NULL;
  }

  //start server
  server.noListenOnLocalhost();
  server.begin();
  Console.println("Listenning for connection");
}

void loop() {
  // put your main code here, to run repeatedly:
 for (int i=0; i<CLIENT_COUNT; i++) {
  if (client[i].is_free) {
    client[i].client = server.accept();
    if (client[i].client) {
      client[i].is_free = false;
      client[i].client.setTimeout(500);
      client[i].client.print(WELCOME_MSG);
      connection_count++;
    }
  }
 }

 if (connection_count >= CLIENT_COUNT) {
  BridgeClient reject_client = server.accept();
  reject_client.print(DENY_MSG);
  reject_client.stop();
 }

  for (int i=0; i<CLIENT_COUNT; i++) {
    if (!client[i].is_free) {
      if (client[i].client.connected()) {
        //int data_count = client[i].client.available();        
        if (client[i].client.available() > 0) {
          int motor_id = -1;
          int intensity = -1;
          int duration = -1;
          client[i].client.read((uint8_t*)&motor_id, 2);
          client[i].client.read((uint8_t*)&intensity, 2);
          client[i].client.read((uint8_t*)&duration, 2);

          if (motor_id != -1 || intensity != -1 || duration != -1) {
            enqueue(motor_id, intensity, duration);
            process_task(i);
          }
        } else {
          //no new data incoming so we process current data
          for (int i=0; i<MOTOR_COUNT; i++) {
            process_task(i);
          }
        }
      } else {
        client[i].is_free = true;
        connection_count--;
        disconnect_client(&(client[i].client));
      }
    }
  }
  //delay(2000);
}

void disconnect_client(BridgeClient *client)
{
  client->stop();
}

void enqueue(int id, int intensity, int duration)
{
  if (id >=0 && id < MOTOR_COUNT) {
    Console.print("ENQUEUE ");
    Console.print(id);
    Console.print(" ");
    Console.print(intensity);
    Console.print(" ");
    Console.print(duration);
    Console.println();
    
    node *new_node = new node;
    Console.print("Pointer ");
    Console.println((long)new_node, HEX);
    if (!new_node) {
      Console.println("Failed to allocate memory");
      return;
    }
    new_node->intensity = intensity;
    new_node->duration = duration;
    new_node->start_time = -1;
    new_node->next = NULL;

    if (motor[id].head == NULL) {
      motor[id].head = new_node;
    } else {
      motor[id].tail->next = new_node;
    }
    motor[id].tail = new_node;
  }
}

void dequeue(int id)
{
  if (motor[id].head == NULL)
    return;

  node *curr_node = motor[id].head;
  motor[id].head = motor[id].head->next;

    Console.print("DEQUEUE ");
    Console.print(id);
    Console.print(" ");
    Console.print(curr_node->intensity);
    Console.print(" ");
    Console.print(curr_node->duration);
    Console.println();  
  delete curr_node;

  if (!motor[id].head) {
    motor[id].tail = NULL;
  }
  
}

node *peek_queue(int id) {
  return (motor[id].head);
}

void operate_motor(int id, int intensity)
{
  switch (id) {
    case 0:
      analogWrite(MOTOR_0_PIN, intensity);
      break;
    case 1:
      analogWrite(MOTOR_1_PIN, intensity);
      break;
    case 2:
      analogWrite(MOTOR_2_PIN, intensity);
      break;
  }
}

void process_task(int id)
{
  node *process_node = peek_queue(id);
  if (process_node) {
    if (process_node->start_time == -1) {
      process_node->start_time = millis();
      operate_motor(id, process_node->intensity);                
      } else {
        if (millis() - process_node->start_time >= process_node->duration) {
          dequeue(id);
          motor[id].motor_status = 0;
          operate_motor(id, 0);
      }
    }
  }
}

