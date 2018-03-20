#include <ESP8266WiFi.h>

//node structure used in queue class
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
//constant value
const int MOTOR_COUNT = 5;
const char ESSID[] = "Arduino_Private_Network";
const char PASSW[] = "UNLVProject";
const char SERVER_ADDR[] = "192.168.240.1";
const int PORT = 5000;
const int MSG_SIZE = 2;
const int DATA_SIZE = 7;

const uint8_t SLAVE_ID = 1;
const uint8_t ORDER = 1;

//-----
//constant values
const uint8_t IDENTIFYING = 100;
const uint8_t ACCEPTED = 101;
const uint8_t REJECTED = 102;
const uint8_t IS_ALIVE = 103;
const uint8_t IAM_ALIVE = 104;
const uint8_t INCOMING_DATA = 200;

const unsigned long CONNECTION_TIME_OUT = 3000;
//-----
bool is_established = false;
bool keep_alive_flag = false;
unsigned long keep_alive_time_keeper = 0;
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
  
  WiFi.mode(WIFI_STA);        //set STATION mode
  WiFi.begin(ESSID, PASSW);   //connect to wifi network
}

void loop() {

  //check if connection is established
  if (client0.connected()) {
    digitalWrite(LED_BUILTIN, HIGH);                //turn off onboard LED

    //incoming message from server
    if (client0.available() > 0) {
      msg = client0.read();
      switch (msg) {
        case IDENTIFYING: {                 //server asks to verify
          msg_to_send[0] = SLAVE_ID;
          msg_to_send[1] = ORDER;
          client0.write(&msg_to_send[0], MSG_SIZE);
          break;
        }
        case ACCEPTED: {                    //server accepts connection
          Serial.println("ARDUINO: Welcome!");
          is_established = true;
          break;
        }
        case REJECTED: {                    //server rejects connection
          Serial.println("ARDUINO: arduino is busy. Goodbye!");
          break;
        }
        case IS_ALIVE: {                    //server sends check_alive message
          //Serial.println("Arduino is asking if alive!");
          client0.write(IAM_ALIVE);
          break;
        }
        case IAM_ALIVE: {                   //server responds to check_alive message
          keep_alive_flag = false;
          keep_alive_time_keeper = 0;
          break;
        }
        case INCOMING_DATA: {               //server sends data
          //process data here
          //add data to queue
          //process data also
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
    } else {  //no incoming message from server at this point
      //check if connection is still alive
      //sends keep_alive message to server
      if (is_established && !keep_alive_flag) {
        if (random(100) <= 30) {
          keep_alive_flag = true;
          keep_alive_time_keeper = millis();
          client0.write(IS_ALIVE);
          
        }
      }
    }
  } else {  //connection is not established at this point
    //attempt to connect to wifi network
    //attempt to connect to server
    digitalWrite(LED_BUILTIN, LOW);
    connect_to_network();
    connect_to_host();
    is_established = false;
  }


  //process all current task for each motor
  for (int i=0; i<MOTOR_COUNT; i++) {
    process_task(i);
  }

  //if keep_alive flag is enable
  //check time lapse since keep_alive sent and keep_alive respond received
  if (keep_alive_flag) {
    if (millis() - keep_alive_time_keeper >= CONNECTION_TIME_OUT) {
      Serial.println("Connection broken!");
      client0.stop();
      keep_alive_flag = false;
      keep_alive_time_keeper = 0;
    }
  }
}

//Funtion to add a task to a queue
//Parameter:  data: data_t: data to add
//Return: none
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

//Function to "look at" a task in the queue
//Parameter:  id: integer: motor id
//Return: *peek_queue: node pointer: a task in the queue
node *peek_queue(int id) {
  if (id >= MOTOR_COUNT) {
    return NULL;
  }
  return (motor[id].head);
}

//Function to remove one task in the queue
//Parameter:  order: integer: motor id
//Return: none
void dequeue(int order)
{
  if (order >= MOTOR_COUNT) {
    return;
  }
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

//Function to examine each task in the queue
//Parameter:  order: unsigned 8-bit integer: motor id
//Return: none
void process_task(uint8_t order)
{
  //Serial.print("Process task called for motor ");
  //Serial.println(order);
  if (order >= MOTOR_COUNT) {
    return;
  }
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

//Function to control motors connected on the board
//Parameter:  order: unsigned 8-bit integer: the motor id
//            intensity: unsigned 8-bit integer: intensity value
//Return: none
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

//Function to connect to wifi network
//Parameter: none
//Return: none
void connect_to_network()
{
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WIFI connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
}

//Function to connect D1 to server
//Parameter: none
//Return: true if sucessfully connect to server
//        false otherwise
bool connect_to_host()
{
  //connect to server
  Serial.println();
  Serial.print("Connecting to server [");
  Serial.print(SERVER_ADDR);
  Serial.print("] on port [");
  Serial.print(PORT);
  Serial.println("]");
  
  return client0.connect(SERVER_ADDR, PORT);
}

