#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>

#define PORT (5000)
#define LED (LED_BUILTIN)
#define USER_COUNT (5)
#define SLAVE_COUNT (5)
#define CONNECTION_TIME_OUT (5000)
#define DATA_PIPE_PROPORTION (3)

#define DATA_SIZE (8)
#define MSG_SIZE (2)

#define USER_ID (0)
#define SLAVE_ID (1)

struct msg_box {
  uint8_t IDENTIFYING;
  uint8_t ACCEPTED;
  uint8_t REJECTED;
  uint8_t IS_ALIVE;
  uint8_t INCOMING_DATA;
  
} msg_to_client = {
  .IDENTIFYING = 100,
  .ACCEPTED = 101,
  .REJECTED = 102,
  .IS_ALIVE = 103,
  .INCOMING_DATA = 200
};




struct client_info {
  bool is_free;
  bool flag;
  unsigned long time_keeper;
  BridgeClient *client;
};

struct data_t {
  uint8_t id;
  uint8_t order;
  uint8_t intensity;
  uint8_t dummy;
  unsigned long duration;
};

struct node {
  data_t data;
  node *next;
};

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




uint8_t msg;
char msg_from_client[MSG_SIZE];

BridgeServer server(PORT);
client_info user[USER_COUNT];
client_info slave[SLAVE_COUNT];

data_pipe dpipe;

unsigned long current_time;

void setup() {
  // put your setup code here, to run once:
  digitalWrite(LED, 1);
  Bridge.begin();
  Console.begin();
  digitalWrite(LED, 0);

  //
  for (int i=0; i<USER_COUNT; i++) {
    user[i].is_free = true;
    user[i].flag = false;
    user[i].time_keeper = 0;
    user[i].client = NULL;
  }

  for (int i=0; i<SLAVE_COUNT; i++) {
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
    msg = msg_to_client.IDENTIFYING;
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
        client.write(msg_to_client.ACCEPTED);
      } else {
        //reject connection
        client.write(msg_to_client.REJECTED);
        disconnect_client(&client);
      }
      
    } else {
      //invalid message
      disconnect_client(&client);
    }
  }

  //process client request
  for (int i=0; i<USER_COUNT; i++) {
    //if the slot is filled
    //check incomming messages
    //Console.print("Check is_free on ");
    //Console.println(i);
    if (!user[i].is_free) {
      //Console.print("Check client on ");
      //Console.println(i);
      if (user[i].client) {
        //Console.print("Check connected on ");
        //Console.println(i);
        if (user[i].client->connected()) {
          //Console.print("Check data available on ");
          //Console.println(i);
          if (user[i].client->available() > 0) {
            //Console.println("THEREISDATA");
            //user[i].client->write(user[i].client->read());
            if (user[i].flag) {
              if (user[i].client->peek() == msg_to_client.IS_ALIVE) {
                user[i].flag = false;
                user[i].time_keeper = 0;
                user[i].client->read();
              } 
            }
            //read data here
            if (user[i].client->available() > 0){
              data_t data;
              int bytes = user[i].client->read((uint8_t*)&data, sizeof (data));
              if (bytes == DATA_SIZE) {
                /*Console.print("Data on [");
                Console.print(i);
                Console.print("] ");
                Console.print(data.id);
                Console.print(" ");
                Console.print(data.order);
                Console.print(" ");
                Console.print(data.intensity);
                Console.print(" ");
                Console.print(data.duration);
                Console.print(" Byte Count: ");
                Console.println(bytes);*/
                dpipe.add_pipe(data);
                //------------
                if (slave[data.id].client) {
                  slave[data.id].client->write(msg_to_client.INCOMING_DATA);
                  int bytes = slave[data.id].client->write((uint8_t*)&data, sizeof (data));
                  Console.print("Bytes sent to [");
                  Console.print(data.id);
                  Console.print("] ");
                  Console.println(bytes);
                }

                
              } else {
                user[i].client->flush();
              }
            }                                    
          } else {
            //if there is no data
            //check if connection is alive
            if (!user[i].flag) {
              
              //ask
              if (random(100) <= 30){
                user[i].flag = true;
                user[i].time_keeper = millis();
                user[i].client->write(msg_to_client.IS_ALIVE);
              }
            } else {
              if (millis() - user[i].time_keeper >= CONNECTION_TIME_OUT) {
                remove_client(USER_ID, i); 
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


  for (int i=0; i<SLAVE_COUNT; i++) {
    if (!slave[i].is_free) {
      if (slave[i].client) {
        if (slave[i].client->connected()) {
          if (slave[i].client->available() > 0) {
            if (slave[i].flag) {
              if (slave[i].client->peek() == msg_to_client.IS_ALIVE) {
                slave[i].flag = false;
                slave[i].time_keeper = 0;
                slave[i].client->read();
              }              
            }
          } else {
            //if there is no data
            //check if connection is alive
            if (!slave[i].flag) {
              
              //ask
              if (random(100) <= 30){
                slave[i].flag = true;
                slave[i].time_keeper = millis();
                slave[i].client->write(msg_to_client.IS_ALIVE);
              }
            } else {
              if (millis() - slave[i].time_keeper >= CONNECTION_TIME_OUT) {
                remove_client(SLAVE_ID, i); 
              }
            }
          }
        } else {
          remove_client(SLAVE_ID, i);
        }
      }
    }
  }
  //Console.print("data in pipe count: ");
  //Console.println(dpipe.get_count());
  
  //process slaves
  /*int data_count = 0;
  data_t data;
  float fraction = (float)dpipe.get_count() / (float)DATA_PIPE_PROPORTION;
  if (fraction < 1) {
    data_count = 1;
  } else {
    data_count = (int)fraction;
  }

  for (int i=0; i<data_count; i++) {
    data_t data;
    data = dpipe.peek_pipe();
    dpipe.delete_pipe();
    
    if (!data.dummy) {
      Console.print("Data from pipe: ");
      Console.print(data.id);
      Console.print(" ");
      Console.print(data.order);
      Console.print(" ");
      Console.print(data.intensity);
      Console.print(" ");
      Console.println(data.duration);
      if (slave[data.id].client) {
        slave[data.id].client->write(msg_to_client.INCOMING_DATA);
        int bytes = slave[data.id].client->write((uint8_t*)&data, sizeof (data));
        Console.print("Bytes sent to [");
        Console.print(data.id);
        Console.print("] ");
        Console.println(bytes);
      }
    }
    //
  }*/

  //method 2
  
  
  //delay(50);
}

void disconnect_client(BridgeClient *client)
{
  client->stop();
}

bool add_client(uint8_t id, uint8_t order, BridgeClient client)
{
  if (id == USER_ID) {
    for (int i=0; i<USER_COUNT; i++) {
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
{
  if (id == USER_ID) {
    disconnect_client(user[index].client);
    delete user[index].client;
    user[index].flag = false;
    user[index].time_keeper = 0;
    user[index].client = NULL;
    user[index].is_free = true;
    
  } else if (id == SLAVE_ID) {
    disconnect_client(slave[index].client);
    delete slave[index].client;
    slave[index].flag = false;
    slave[index].time_keeper = 0;
    slave[index].client = NULL;
    slave[index].is_free = true;
  }
  
}

