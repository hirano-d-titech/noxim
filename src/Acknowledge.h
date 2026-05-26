#ifndef ACKNOWLEDGE_H
#define ACKNOWLEDGE_H

#include "DataStructs.h"
#include "GlobalParams.h"
#include "Utils.h"
#include <systemc.h>
#include <vector>
#include <map>
#include <queue>

struct PendingAck {
  int src_id;
  int dst_id;
  int packet_id;
  bool is_nack;
  int remaining_delay;
};

SC_MODULE(Acknowledge) {
public:
  sc_in_clk clock;
  sc_in<bool> reset;

  sc_vector<sc_in<Ack>> req_rx;
  sc_vector<sc_out<Ack>> ack_tx;

  SC_CTOR(Acknowledge) {
    int num_tiles = GlobalParams::mesh_dim_x * GlobalParams::mesh_dim_y;
    req_rx.init(num_tiles);
    ack_tx.init(num_tiles);
    ack_queues.resize(num_tiles);

    SC_METHOD(update);
    sensitive << reset;
    sensitive << clock.pos();
  }

  void update();

private:
  std::vector<PendingAck> pending_acks;
  std::vector<std::queue<Ack>> ack_queues;
};

#endif // ACKNOWLEDGE_H