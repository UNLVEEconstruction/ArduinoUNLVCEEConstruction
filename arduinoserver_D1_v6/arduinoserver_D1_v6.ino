#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <SD.h>
#include <SPI.h>

//const stuffs
const char* ssid = "Arduino_Private_Network";
const char* passw = "UNLVProject";

IPAddress ip(192, 168, 240, 1);
IPAddress gateway(192, 168, 240, 1);
IPAddress subnet(255, 255, 255, 0);

File myFile;

//---
#define PORT (5000)                       //port number
#define LED (LED_BUILTIN)                 //default LED on arduino
#define USER_COUNT (5)                    //maximum number of user nodes
#define SLAVE_COUNT (5)                   //maximun number of slave nodes
#define CONNECTION_TIME_OUT (5000)        //wating time before disconnecting unrespoding clients

#define DATA_SIZE (8)                     //size of each incoming data packet
#define MSG_SIZE (2)                      //size of message packet

#define USER_ID (0)                       //user id
#define SLAVE_ID (1)                      //slave id

#define BUF_SIZE (256)                    //size of buffer

#define SD_ATTEMPT (5)                    //max number of attempting to open sd card

//-----NOT USED------
#define LF (10) //line feed
#define CR (13) //carriage return
#define SPACE (32)
#define TAB (9)

//---------------------
//client data structure
struct client_info {
  bool is_free;                 //if false: a client is connected on this object
  bool flag;                    //used to check if there is a check_alive_request in processing
  unsigned long time_keeper;    //time since the check_alive_request is sent out on this object
  WiFiClient *client;         //client object
};

//data structure
struct data_t {
  uint8_t id;                   //used to distinguish slaves
  uint8_t order;                //used to distinguish motors
  uint8_t intensity;            //intensity value (0-255)
  uint8_t dummy;                //used to distingush request
  //0: direct request
  //1: oped file on sd card request
  unsigned long duration;       //duration value
};

struct pair_mapping {
  uint8_t id;
  uint8_t order;
  bool operator== (const pair_mapping &second) {
    return (this->id == second.id) && (this->order == second.order);
  }
};

//node structure used in queue (data_pipe) class
//NOT USED
struct node {
  data_t data;
  node *next;
};

//queue data structure
//NOT USED
class data_pipe {
  private:
    node *head;
    node *tail;
    int count;
  public:
    data_pipe() {
      head = NULL;
      tail = NULL;
      count = 0;
    }

    int get_count() {
      return count;
    }

    bool add_pipe(data_t data) {

      node *new_node = new node;

      if (!new_node) {
        return false;
      }
      count++;
      Serial.print("Added: ");
      Serial.print(data.id);
      Serial.print(" ");
      Serial.print(data.order);
      Serial.print(" ");
      Serial.print(data.intensity);
      Serial.print(" ");
      Serial.print(data.dummy);
      Serial.print(" ");
      Serial.println(data.intensity);

      new_node->next = NULL;
      new_node->data = data;

      if (head == NULL) {
        head = new_node;
      } else {
        tail->next = new_node;
      }
      tail = new_node;
    }


    data_t peek_pipe() {
      data_t result;
      result.dummy = 1;
      if (head != NULL) {
        result = head->data;
      }
      return result;
    }

    void delete_pipe() {
      if (head == NULL) {
        return;
      }

      count--;
      node *curr_node = head;
      head = head->next;
      delete curr_node;

      if (!head) {
        tail = NULL;
      }
    }

};

//-----
//constant values
const uint8_t IDENTIFYING = 100;
const uint8_t ACCEPTED = 101;
const uint8_t REJECTED = 102;
const uint8_t IS_ALIVE = 103;
const uint8_t IAM_ALIVE = 104;
const uint8_t INCOMING_DATA = 200;
const uint8_t PROFILE_COUNT = 201;
const uint8_t PROFILE_INDEX = 202;

const char *PRE_FILE = "";    //directory of profiles on sd card
const char *POST_FILE = ".txt";       //file extention
//-----

char filename[30];                    //profile to open on sd card
char buff[BUF_SIZE];

uint8_t msg;                          //message to clients
uint8_t profile_count;                //number of profiles on sd card
//read from metadata.txt

char msg_from_client[MSG_SIZE];       //request message from clients

WiFiServer server(PORT);            //server object on port 5000
client_info user[USER_COUNT];         //user objects holder
client_info slave[SLAVE_COUNT];       //slave objects holder

data_pipe dpipe;                      //queue object - NOT USED

unsigned long current_time;           //used for time stamp

int i;
//---



void setup() {
  //begin serial to debugging
  Serial.begin(9600);


  //setup wifi network
  WiFi.softAPConfig(ip, gateway, subnet);
  WiFi.softAP(ssid, passw);


  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);


  //initialize SD card

  Serial.print("Initializing SD card...");

  if (!SD.begin(4)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  //open sd card
  sprintf(filename, "%s%s%s", PRE_FILE, "metadata", POST_FILE);

  i = 0;
  profile_count = 0;
  while (i < SD_ATTEMPT) {
    File file = SD.open(filename, FILE_READ);
    if (file) {
      profile_count = file.parseInt();
      break;
    }
    i++;
    delay(50);
  }

  Serial.print("Profile Count: ");
  Serial.println(profile_count);

  //initialize user objects
  for (int i = 0; i < USER_COUNT; i++) {
    user[i].is_free = true;
    user[i].flag = false;
    user[i].time_keeper = 0;
    user[i].client = NULL;
  }

  //initialize slave object
  for (int i = 0; i < SLAVE_COUNT; i++) {
    slave[i].is_free = true;
    slave[i].flag = false;
    slave[i].time_keeper = 0;
    slave[i].client = NULL;
  }

  //start server
  server.begin();
  Serial.println("Listenning for connection");

}

void loop() {
  // put your main code here, to run repeatedly:
    //accept incoming connection
  WiFiClient client = server.available();
  if (client) {
    //if there is a connection
    //ask client to identify itself
    //(user or slave)
    msg = IDENTIFYING;
    client.write(msg);
    current_time = millis();
    while (client.available() == 0) {
      //wait for message
      if (millis() - current_time >= CONNECTION_TIME_OUT / 2) {
        client.stop();
        break;
      }
    }
    //there is a message
    if (client.read((uint8_t*)&msg_from_client, MSG_SIZE) == MSG_SIZE) {
      //valid message; continue to process
      //get higher 8 bits
      uint8_t identifier = (uint8_t)msg_from_client[0];
      Serial.print("IDENTIFIER: ");
      Serial.println(identifier);
      //get lower 8 bits
      uint8_t order = (uint8_t)msg_from_client[1];
      Serial.print("ORDER: ");
      Serial.println(order);

      if (add_client(identifier, order, client)) {
        //accept connection
        client.write(ACCEPTED);
      } else {
        //reject connection
        client.write(REJECTED);
        disconnect_client(&client);
      }
    } else {
      //invalid message
      disconnect_client(&client);
    }
  }

  //process client request here
  for (int i = 0; i < USER_COUNT; i++) {
    if (!user[i].is_free) {
      if (user[i].client) {
        if (user[i].client->connected()) {
          if (user[i].client->available() > 0) {
            msg = user[i].client->read();
            switch (msg) {
              case IS_ALIVE: {                      //check_alive message from client
                  user[i].client->write(IAM_ALIVE);   //write back to client that connection is still alive
                  break;
                }
              case IAM_ALIVE: {                     //check_alive respond from client
                  if (user[i].flag) {                 //reset check_alive process
                    user[i].flag = false;
                    user[i].time_keeper = 0;
                  }
                  break;
                }
              case INCOMING_DATA: {                 //data is comming from user
                  //read data
                  //reply data to appropriate slave
                  Serial.print("Incoming data from [");
                  Serial.print(i);
                  Serial.println("]");
                  while (user[i].client->available() <= 0);
                  data_t data;
                  int bytes = user[i].client->read((uint8_t*)&data, sizeof(data));
                  Serial.print("Byte read ");
                  Serial.print(bytes);
                  Serial.println("");
                  if (bytes == DATA_SIZE) {     //direct request
                    if (data.dummy == 0) {
                      if (slave[data.id].client && data.id < SLAVE_COUNT) {
                        slave[data.id].client->write(INCOMING_DATA);
                        int bytes = slave[data.id].client->write((uint8_t*)&data, sizeof (data));
                        Serial.print("Sent ");
                        Serial.print(data.id);
                        Serial.print(" ");
                        Serial.print(data.order);
                        Serial.print(" ");
                        Serial.print(data.intensity);
                        Serial.print(" ");
                        Serial.println(data.duration);

                        Serial.print("Bytes sent to [");
                        Serial.print(data.id);
                        Serial.print("] ");
                        Serial.println(bytes);
                      }
                    } else {                    //request to read from sd card
                      sprintf(filename, "%s%d%s", PRE_FILE, data.id, POST_FILE);
                      File file = SD.open(filename, FILE_READ);
                      if (file) {
                        int i = 0;
                        char *endptr;
                        while (file.available()) {
                          i = 0;
                          do {
                            buff[i] = file.read();
                            if (buff[i] == char(-1)) {
                              break;
                            }
                            i++;
                          } while (buff[i - 1] != LF);
                          buff[i] = '\0';

                          data.id = strtol(buff, &endptr, 10);
                          data.order = strtol(endptr, &endptr, 10);
                          data.intensity = strtol(endptr, &endptr, 10);
                          data.dummy = 0;
                          data.duration = strtol(endptr, &endptr, 10);

                          Serial.println("Calling motor mapping");
                          motor_mapping(&data.id, &data.order);
                          
                          if (data.id < SLAVE_COUNT && slave[data.id].client) {
                            slave[data.id].client->write(INCOMING_DATA);
                            slave[data.id].client->write((uint8_t*)&data, sizeof(data));
                            Serial.print("Sent ");
                            Serial.print(data.id);
                            Serial.print(" ");
                            Serial.print(data.order);
                            Serial.print(" ");
                            Serial.print(data.intensity);
                            Serial.print(" ");
                            Serial.println(data.duration);
                          }

                        }

                      } else {
                        //file not found
                        sprintf(filename, "%d%s", data.id, POST_FILE);
                        Serial.print(filename);
                        Serial.println(" is not found!!!");
                      }
                    }
                  } else {
                    user[i].client->flush();
                  }
                  break;
                }

              case PROFILE_COUNT: {       //request for number of profiles on sd card
                  user[i].client->write(PROFILE_COUNT);
                  user[i].client->write(profile_count);
                  break;
                }
              default: {
                  Serial.println("Unknown message");
                }
            }
          } else {
            //if there is no data
            //check if connection is alive
            if (!user[i].flag) {
              //send check_alive message
              if (random(100) <= 30) {
                user[i].flag = true;
                user[i].time_keeper = millis();
                user[i].client->write(IS_ALIVE);
              }
            }
          }
        } else {
          //remove client
          remove_client(USER_ID, i);
        }
      }
    }
  }

  for (int i = 0; i < USER_COUNT; i++) {
    if (user[i].flag) {
      if (millis() - user[i].time_keeper >= CONNECTION_TIME_OUT) {
        remove_client(USER_ID, i);
      }
    }
  }

  for (int i = 0; i < SLAVE_COUNT; i++) {
    if (!slave[i].is_free) {
      if (slave[i].client) {
        if (slave[i].client->connected()) {
          if (slave[i].client->available() > 0) {
            //--
            msg = slave[i].client->read();
            switch (msg) {
              case IS_ALIVE: {
                  slave[i].client->write(IAM_ALIVE);
                  break;
                }
              case IAM_ALIVE: {
                  slave[i].flag = false;
                  slave[i].time_keeper = 0;
                  break;
                }
              default: {
                  Serial.print("[");
                  Serial.print(i);
                  Serial.println("] Slave received unknown message!");
                }
            }
            //--
          } else {
            //if there is no data
            //check if connection is alive
            if (!slave[i].flag) {

              //ask
              if (random(100) <= 30) {
                slave[i].flag = true;
                slave[i].time_keeper = millis();
                slave[i].client->write(IS_ALIVE);
              }
            }
          }
        } else {
          remove_client(SLAVE_ID, i);
        }
      }
    }
  }

  //check time lapse between keep_alive message is sent
  //and keep_alive respond received
  for (int i = 0; i < SLAVE_COUNT; i++) {
    if (slave[i].flag) {
      if (millis() - slave[i].time_keeper >= CONNECTION_TIME_OUT) {
        remove_client(SLAVE_ID, i);
      }
    }
  }
}

//
void disconnect_client(WiFiClient * client)
//Function to disconnect a client
//Parameter: *client: WiFiClient //reference to a client object
//Return: none
{
  client->stop();
}

bool add_client(uint8_t id, uint8_t order, WiFiClient client)
//Function to add an incoming client to the data array
//Parameters: id: unsigned 8-bit integer    //user or slave
//            order: unsigned 8-bit integer //slot number
//            client: WiFiClient          //client object
//Return: true if adding sucessfully
//        false otherwise
{
  if (id == USER_ID) {
    for (int i = 0; i < USER_COUNT; i++) {
      if (user[i].is_free) {
        //if there slot; add to it
        user[i].client = new WiFiClient(client);
        user[i].is_free = false;
        Serial.println("ADDeD");
        return true;
      }
    }
  } else if (id == SLAVE_ID) {
    //
    if (slave[order].is_free) {
      slave[order].client = new WiFiClient(client);
      slave[order].is_free = false;
      Serial.println("ADDeD");
      return true;
    }
  }
  return false;
}

void remove_client(uint8_t id, uint8_t index)
//Function to remove a client from the list
//  to free up space for next incoming client
//  handle both slaves and users
//Parameters: id: unsinged 8-bit integer    //user or slave
//            index: unsigned 8-bit integer //slot number
//Return: none
{
  if (id == USER_ID) {

    if (user[index].client) {
      disconnect_client(user[index].client);
      delete user[index].client;
    }
    user[index].flag = false;
    user[index].time_keeper = 0;
    user[index].client = NULL;
    user[index].is_free = true;

  } else if (id == SLAVE_ID) {

    if (slave[index].client) {
      disconnect_client(slave[index].client);
      delete slave[index].client;
    }
    slave[index].flag = false;
    slave[index].time_keeper = 0;
    slave[index].client = NULL;
    slave[index].is_free = true;
  }

}

void motor_mapping(uint8_t *id, uint8_t *order)
{
  static const uint8_t ROW = 4;
  static const uint8_t COL = 3;

  static pair_mapping infile_motor_table[ROW][COL] = { 
    { {255, 255}, {0, 2}, {255, 255} },
    { {1, 1}, {1, 2}, {1, 3} },
    { {2, 1}, {2, 2}, {2, 3} },
    { {3, 1}, {3, 2}, {3, 3} }
  };
  
  static pair_mapping real_motor_table[ROW][COL] = { 
    { {255, 255}, {1, 4}, {255, 255} },
    { {0, 0}, {0, 1}, {0, 2} },
    { {0, 3}, {0, 4}, {1, 0} },
    { {1, 1}, {1, 2}, {1, 3} }
  };

  pair_mapping data;
  data.id = *id;
  data.order = *order;

  for (int i=0; i<ROW; i++) {
    for (int j=0; j<COL; j++) {
      if (data == infile_motor_table[i][j]) {
        *id = real_motor_table[i][j].id;
        *order = real_motor_table[i][j].order;
      }
    }
  }

  Serial.print("ID: ");
  Serial.println(data.id);
  Serial.print("ORDER: ");
  Serial.println(data.order);

}
