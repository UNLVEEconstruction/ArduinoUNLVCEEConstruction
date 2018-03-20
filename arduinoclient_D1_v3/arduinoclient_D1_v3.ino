#include <ESP8266WiFi.h>

struct node {
  int intensity;
  unsigned long duration;
  unsigned long start_time;
  node *next;
};


struct motor_control_info {
  node *head;
  node *tail;
};

struct data_t {
  uint8_t id;
  uint8_t order;
  uint8_t intensity;
  uint8_t dummy;
  unsigned long duration;
} data = {
    .id = 0,
    .order = 0,
    .intensity = 0,
    .dummy = 0,
    .duration = 0
};

//------------
const int MOTOR_COUNT = 5;
const char ESSID[] = "Arduino_Private_Network";
const char PASSW[] = "UNLVProject";
const char SERVER_ADDR[] = "192.168.240.1";
const int PORT = 5000;
const int MSG_SIZE = 2;
const int DATA_SIZE = 7;

const uint8_t SLAVE_ID = 1;
const uint8_t ORDER = 2;

//-----
const uint8_t IDENTIFYING = 100;
const uint8_t ACCEPTED = 101;
const uint8_t REJECTED = 102;
const uint8_t IS_ALIVE = 103;

const uint8_t INCOMING_DATA = 200;
//-----
unsigned long r = 0;
unsigned long m = 0;

const int motor0 = D2;
const int motor1 = D5;
const int motor2 = D6;
const int motor3 = D7;
const int motor4 = D8;

uint8_t msg;
uint8_t msg_to_send[MSG_SIZE];
int error_code = 0;
bool flag = false;

WiFiClient client0;

//------------
motor_control_info motor[MOTOR_COUNT];

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  
  pinMode(motor0, OUTPUT);
  pinMode(motor1, OUTPUT);
  pinMode(motor2, OUTPUT);
  pinMode(motor3, OUTPUT);
  pinMode(motor4, OUTPUT);
  analogWriteRange(255);
  analogWrite(motor0, 0);
  analogWrite(motor1, 0);
  analogWrite(motor2, 0);
  analogWrite(motor3, 0);
  analogWrite(motor4, 0);
  
  Serial.begin(115200);
  delay(10);


  Serial.println();
  Serial.print("Connecting to network [");
  Serial.print(ESSID);
  Serial.println("]");

  digitalWrite(LED_BUILTIN, LOW); //low is on
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ESSID, PASSW);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WIFI connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, HIGH);

  //connect to server
  Serial.println();
  Serial.print("Connecting to server [");
  Serial.print(SERVER_ADDR);
  Serial.print("] on port [");
  Serial.print(PORT);
  Serial.println("]");
  
  flag = client0.connect(SERVER_ADDR, PORT);
  if (flag) {
    Serial.println("Conection established.");
  } else {
    Serial.println("Conection failed.");
  }

}

void loop() {
  if (client0.connected()) {
    if (client0.available() > 0) {
      msg = client0.read();
      switch (msg) {
        case IDENTIFYING: {
          msg_to_send[0] = SLAVE_ID;
          msg_to_send[1] = ORDER;
          client0.write(&msg_to_send[0], MSG_SIZE);
          break;
        }
        case ACCEPTED: {
          Serial.println("ARDUINO: Welcome!");
          break;
        }
        case REJECTED: {
          Serial.println("ARDUINO: arduino is busy. Goodbye!");
          break;
        }
        case IS_ALIVE: {
          //Serial.println("Arduino is asking if alive!");
          client0.write(msg);
          break;
        }
        case INCOMING_DATA: {
          
          Serial.println("Recieved signal for incomming data");
          while (client0.available() <= 0);
          int bytes = client0.read((uint8_t*)&data, sizeof data);
          Serial.print("Data received: ");
          Serial.print(data.id);
          Serial.print(" ");
          Serial.print(data.order);
          Serial.print(" ");
          Serial.print(data.intensity);
          Serial.print(" ");
          Serial.print(data.duration);
          Serial.print(" Byte read: ");
          Serial.println(bytes);
          enqueue(data);
          process_task(data.order);
          r++;
          if (data.id > 1 || data.order > 5 || data.intensity != 100 || data.duration != 2000)
            m++;
          break;
        }
        defaul: {
          Serial.println("Unknown message from server");
        }
      }
    } else {
      for (int i=0; i<MOTOR_COUNT; i++) {
        process_task(i);
      }
    }
  }


 /*Serial.print("Data recieved count: ");
 Serial.println(r);
 Serial.print("Data missed count: ");
 Serial.println(m);*/
}

void enqueue(data_t data)
{
    if (data.order < 0 || data.order >= MOTOR_COUNT) {
      return;
    }
    Serial.print("ENQUEUE ");
    Serial.print(data.order);
    Serial.print(" ");
    Serial.print(data.intensity);
    Serial.print(" ");
    Serial.print(data.duration);
    Serial.println();
    
    node *new_node = new node;
    if (!new_node) {
      Serial.println("Failed to allocate memory");
      return;
    }
    new_node->intensity = data.intensity;
    new_node->duration = data.duration;
    new_node->start_time = 0;
    new_node->next = NULL;

    if (motor[data.order].head == NULL) {
      motor[data.order].head = new_node;
    } else {
      motor[data.order].tail->next = new_node;
    }
    motor[data.order].tail = new_node;
  
}

node *peek_queue(int id) {
  return (motor[id].head);
}

void dequeue(int order)
{
  Serial.print("Dequeue motor called for motor ");
  Serial.println(order); 
  if (motor[order].head == NULL)
    return;

  node *curr_node = motor[order].head;
  motor[order].head = motor[order].head->next;
  
  delete curr_node;

  if (motor[order].head == NULL) {
    motor[order].tail = NULL;
  }
  
}


void process_task(uint8_t order)
{
  //Serial.print("Process task called for motor ");
  //Serial.println(order);
  node *process_node = peek_queue(order);
  if (process_node) {
    if (process_node->start_time == 0) {
      process_node->start_time = millis();
      operate_motor(order, process_node->intensity);                
      } else {
        if (millis() - process_node->start_time >= process_node->duration) {
          dequeue(order);
          operate_motor(order, 0);
      }
    }
  }
}

void operate_motor(uint8_t order, uint8_t intensity)
{
  //Serial.print("Operate motor called for motor ");
  //Serial.println(order);
  switch (order) {
    case 0:
      analogWrite(motor0, intensity);
      //analogWrite(motor0, intensity);
      break;
    case 1:
      analogWrite(motor1, intensity);
      //analogWrite(motor1, intensity);
      break;
    case 2:
      analogWrite(motor2, intensity);
      //analogWrite(motor2, intensity);
      break;
    case 3:
      analogWrite(motor3, intensity);
      break;
    case 4:
      analogWrite(motor4, intensity);
      break;
  }
}
