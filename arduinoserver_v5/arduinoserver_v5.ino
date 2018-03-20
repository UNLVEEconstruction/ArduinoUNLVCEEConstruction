#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>
#include <FileIO.h>


#define PORT (5000)                       //port number
#define LED (LED_BUILTIN)                 //default LED on arduino
#define USER_COUNT (5)                    //maximum number of user nodes
#define SLAVE_COUNT (5)                   //maximun number of slave nodes
#define CONNECTION_TIME_OUT (5000)        //wating time before disconnecting unrespoding clients

#define DATA_SIZE (8)                     //size of each incoming data packet
#define MSG_SIZE (2)                      //size of message packet

#define USER_ID (0)                       //user id
#define SLAVE_ID (1)                      //slave id

#define BUF_SIZE (1024)

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
  BridgeClient *client;         //client object
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
      Console.print("Added: ");
      Console.print(data.id);
      Console.print(" ");
      Console.print(data.order);
      Console.print(" ");
      Console.print(data.intensity);
      Console.print(" ");
      Console.print(data.dummy);
      Console.print(" ");
      Console.println(data.intensity);

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

const char *PRE_FILE = "/mnt/sd/";    //directory of profiles on sd card
const char *POST_FILE = ".txt";       //file extention
//-----

char filename[30];                    //profile to open on sd card
char buff[BUF_SIZE];

uint8_t msg;                          //message to clients
uint8_t profile_count;                //number of profiles on sd card
//read from metadata.txt

char msg_from_client[MSG_SIZE];       //request message from clients

BridgeServer server(PORT);            //server object on port 5000
client_info user[USER_COUNT];         //user objects holder
client_info slave[SLAVE_COUNT];       //slave objects holder

data_pipe dpipe;                      //queue object - NOT USED

unsigned long current_time;           //used for time stamp


//setup function
void setup() {
  //setup libriraries
  pinMode(LED, OUTPUT);
  digitalWrite(LED, 1);
  Bridge.begin();
  Console.begin();
  FileSystem.begin();
  digitalWrite(LED, 0);

  //open sd card
  sprintf(filename, "%s%s%s", PRE_FILE, "metadata", POST_FILE);
  File file = FileSystem.open(filename, FILE_READ);
  if (file) {
    profile_count = file.parseInt();
  } else {
    profile_count = 0;    //cannot open metadata.txt at this point
  }


  Console.print("Profile Count: ");
  Console.println(profile_count);

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
  server.noListenOnLocalhost();
  server.begin();
  Console.println("Listenning for connection");

}


void loop() {
  //accept incoming connection
  BridgeClient client = server.accept();
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
      Console.print("IDENTIFIER: ");
      Console.println(identifier);
      //get lower 8 bits
      uint8_t order = (uint8_t)msg_from_client[1];
      Console.print("ORDER: ");
      Console.println(order);

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
                  Console.print("Incoming data from [");
                  Console.print(i);
                  Console.println("]");
                  while (user[i].client->available() <= 0);
                  data_t data;
                  int bytes = user[i].client->read((uint8_t*)&data, sizeof(data));
                  Console.print("Byte read ");
                  Console.print(bytes);
                  Console.println("");
                  if (bytes == DATA_SIZE) {     //direct request
                    if (data.dummy == 0) {
                      if (slave[data.id].client && data.id < SLAVE_COUNT) {
                        slave[data.id].client->write(INCOMING_DATA);
                        int bytes = slave[data.id].client->write((uint8_t*)&data, sizeof (data));
                        Console.print("Sent ");
                        Console.print(data.id);
                        Console.print(" ");
                        Console.print(data.order);
                        Console.print(" ");
                        Console.print(data.intensity);
                        Console.print(" ");
                        Console.println(data.duration);

                        Console.print("Bytes sent to [");
                        Console.print(data.id);
                        Console.print("] ");
                        Console.println(bytes);
                      }
                    } else {                    //request to read from sd card
                      sprintf(filename, "%s%d%s", PRE_FILE, data.id, POST_FILE);
                      File file = FileSystem.open(filename, FILE_READ);
                      if (file) {
                        while (file.available()) {
                          data.id = file.parseInt();
                          data.order = file.parseInt();
                          data.intensity = file.parseInt();
                          data.dummy = 0;
                          data.duration = file.parseInt();
                          if (data.id < SLAVE_COUNT && slave[data.id].client) {
                            slave[data.id].client->write(INCOMING_DATA);
                            slave[data.id].client->write((uint8_t*)&data, sizeof(data));
                            Console.print("Sent ");
                            Console.print(data.id);
                            Console.print(" ");
                            Console.print(data.order);
                            Console.print(" ");
                            Console.print(data.intensity);
                            Console.print(" ");
                            Console.println(data.duration);
                          }
                        }


                      } else {
                        //file not found
                        sprintf(filename, "%d%s", data.id, POST_FILE);
                        Console.print(filename);
                        Console.println(" is not found!!!");
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
                  Console.println("Unknown message");
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
                  Console.print("[");
                  Console.print(i);
                  Console.println("] Slave received unknown message!");
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

void disconnect_client(BridgeClient * client)
//Function to disconnect a client
//Parameter: *client: BridgeClient //reference to a client object
//Return: none
{
  client->stop();
}

bool add_client(uint8_t id, uint8_t order, BridgeClient client)
//Function to add an incoming client to the data array
//Parameters: id: unsigned 8-bit integer    //user or slave
//            order: unsigned 8-bit integer //slot number
//            client: BridgeClient          //client object
//Return: true if adding sucessfully
//        false otherwise
{
  if (id == USER_ID) {
    for (int i = 0; i < USER_COUNT; i++) {
      if (user[i].is_free) {
        //if there slot; add to it
        user[i].client = new BridgeClient(client);
        user[i].is_free = false;
        Console.println("ADDeD");
        return true;
      }
    }
  } else if (id == SLAVE_ID) {
    //
    if (slave[order].is_free) {
      slave[order].client = new BridgeClient(client);
      slave[order].is_free = false;
      Console.println("ADDeD");
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

