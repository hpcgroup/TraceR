#include "string.h"
#include "codes/map_messages.h"

std::vector< std::map< uint64_t, MsgRecord > > msgList; 

extern "C" {

void createMsgMap(int numNics) {
  msgList.resize(numNics);
}

void addMsgInfo(uint64_t nic_num, uint64_t msg_id, uint64_t recvSize,
            int eventSize, char * data) {
  
  MsgRecord & msg = msgList[nic_num][msg_id];
  msg.recvBytes += recvSize;
  if(eventSize != -1 && msg.remoteEventSize == -1) {
    msg.remoteEventSize = eventSize;
    memcpy(msg.data, data, eventSize);
  }
}

void getMsgInfo(uint64_t nic_num, uint64_t msg_id, uint64_t &recvSize, 
            int &eventSize, char *& data) { 
  MsgRecord & msg = msgList[nic_num][msg_id];
  recvSize = msg.recvBytes;
  eventSize = msg.remoteEventSize;
  data = msg.data;
}

void deleteMsgInfo(uint64_t nic_num, uint64_t msg_id) {
  msgList[nic_num].erase(msg_id);
}

} //extern C
