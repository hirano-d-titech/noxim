#include "Acknowledge.h"

void Acknowledge::update() {
  if (reset.read()) {
    pending_acks.clear();
    for (int i = 0; i < ack_tx.size(); ++i) {
      ack_tx[i].write(Ack()); // fill with invalid ACK to clear signals on reset
    }
  } else {
    // clear all output
    for (int i = 0; i < ack_tx.size(); ++i) {
      ack_tx[i].write(Ack());
    }

    // check requested Ack and pending
    for (int i = 0; i < req_rx.size(); ++i) {
      if (req_rx[i].event()) {
        const Ack &ack = req_rx[i].read();
        if (ack.isValid()) {
          int manhattan_distance = getManhattanDistance(ack.src_id, ack.dst_id);

          PendingAck pending;
          pending.src_id = ack.src_id;
          pending.dst_id = ack.dst_id;
          pending.remaining_delay = manhattan_distance;

          pending_acks.push_back(pending);
        }
      }
    }

    // update pending ACKs and send if delay is completed
    for (auto it = pending_acks.begin(); it != pending_acks.end(); ) {
      it->remaining_delay--;
      if (it->remaining_delay <= 0) {
        ack_tx[it->src_id].write(Ack(it->src_id, it->dst_id));
        it = pending_acks.erase(it);
      } else {
        ++it;
      }
    }
  }
}