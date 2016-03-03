#include "stdio.h"
#include "string.h"
#include "codes/map_messages.h"
#include "assert.h"

std::vector< std::map< uint64_t, MsgRecord > > msgList; 

extern "C" {

void createMsgMap(int numNics) {
  if(!msgList.size()) {
    msgList.resize(numNics);
  }
}

void checkNonZero() {
  for(int i = 0; i < msgList.size(); i++) {
    if(msgList[i].size() != 0) {
      printf("Non zero found at %d size %d\n", i, msgList[i].size());
    }
  }
}

int addMsgInfo(uint64_t nic_num, uint64_t msg_id, uint64_t recvSize,
            int eventSize, char * data) {
  MsgRecord & msg = msgList[nic_num][msg_id];
  msg.recvBytes += recvSize;
  if(eventSize != 0 && msg.remoteEventSize == 0) {
    msg.remoteEventSize = eventSize;
    memcpy(msg.data, data, eventSize);
    return 1;
  }
  return 0;
}

void getMsgInfo(uint64_t nic_num, uint64_t msg_id, uint64_t *recvSize,
            int *eventSize, char ** data) {
  std::map<uint64_t, MsgRecord >::iterator it = msgList[nic_num].find(msg_id);
  if(it == msgList[nic_num].end()) {
    printf("Request for unavailable message\n");
    assert(0);
    return;
  }
  MsgRecord & msg = (it->second);
  *recvSize = msg.recvBytes;
  *eventSize = msg.remoteEventSize;
  *data = msg.data;
}

void decreaseMsgInfo(uint64_t nic_num, uint64_t msg_id, uint64_t recvSize,
            int reset_data) {
  std::map<uint64_t, MsgRecord >::iterator it = msgList[nic_num].find(msg_id);
  if(it == msgList[nic_num].end()) {
    printf("Decrease called for non existing msg\n");
    assert(0);
    return;
  }
  MsgRecord & msg = (it->second);
  if(msg.recvBytes < recvSize) {
    printf("Msg.recvbytes went below 0\n");
    assert(0);
  }
  msg.recvBytes -= recvSize;
  if(msg.recvBytes == 0) {
    msgList[nic_num].erase(it);
    return;
  }
  if(reset_data) {
    msg.remoteEventSize = 0;
  }
}

void deleteMsgInfo(uint64_t nic_num, uint64_t msg_id) {
  msgList[nic_num].erase(msg_id);
}

} //extern C
