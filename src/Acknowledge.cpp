#include "Acknowledge.h"

void Acknowledge::update() {
  if (sc_time_stamp() == SC_ZERO_TIME) {
    return;
  }

  if (reset.read()) {
    pending_acks.clear();
    for (size_t i = 0; i < ack_queues.size(); ++i) {
      while (!ack_queues[i].empty()) {
        ack_queues[i].pop();
      }
    }
    for (int i = 0; i < ack_tx.size(); ++i) {
      ack_tx[i].write(Ack()); // fill with invalid ACK to clear signals on reset
    }
  } else {
    // find new Ack
    for (int i = 0; i < req_rx.size(); ++i) {
      const Ack &ack = req_rx[i].read();
      if (ack.isValid()) {
        // prepending dupe flit generating
        bool exists = false;
        for (const auto &p : pending_acks) {
          if (p.src_id == ack.src_id && p.dst_id == ack.dst_id && p.packet_id == ack.packet_id) {
            exists = true;
            break;
          }
        }
        if (!exists) {
          // index check for safety
          if (ack.src_id >= 0 && ack.src_id < (int)ack_queues.size() && ack.dst_id >= 0 && ack.dst_id < (int)req_rx.size()) {
            int manhattan_distance = getManhattanDistance(ack.src_id, ack.dst_id);

            PendingAck pending;
            pending.src_id = ack.src_id;
            pending.dst_id = ack.dst_id;
            pending.packet_id = ack.packet_id;
            pending.is_nack = ack.is_nack;
            pending.remaining_delay = manhattan_distance;

            pending_acks.push_back(pending);
          }
        }
      }
    }

    // decreasing delay and queuing sending ACKs
    for (auto it = pending_acks.begin(); it != pending_acks.end(); ) {
      it->remaining_delay--;
      if (it->remaining_delay <= 0) {
        if (it->src_id >= 0 && it->src_id < (int)ack_queues.size()) {
          Ack new_ack(it->src_id, it->dst_id, it->packet_id, it->is_nack);
          ack_queues[it->src_id].push(new_ack);
        }
        it = pending_acks.erase(it);
      } else {
        ++it;
      }
    }

    // 1 Ack per cycle per output port
    for (int i = 0; i < ack_tx.size(); ++i) {
      if (!ack_queues[i].empty()) {
        ack_tx[i].write(ack_queues[i].front());
        ack_queues[i].pop();
      } else {
        ack_tx[i].write(Ack());
      }
    }
  }
}