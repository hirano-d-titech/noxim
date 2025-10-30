#include "Acknowledge.h"

void Acknowledge::main_process() {
  while (true) {
    wait();
    for (int i = 0; i < req_rx.size(); ++i) {
      if (req_rx[i].event()) {
        const Ack &ack = req_rx[i].read();
        send_ack(ack);
      }
    }
  }
}

void Acknowledge::send_ack(const Ack &ack) {
  int manhattan_distance =
      getManhattanDistance(ack.src_id, ack.dst_id);
  wait(manhattan_distance);
  ack_tx[ack.src_id].write(true);
}
