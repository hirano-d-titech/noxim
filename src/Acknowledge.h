#ifndef ACKNOWLEDGE_H
#define ACKNOWLEDGE_H

#include "DataStructs.h"
#include "GlobalParams.h"
#include "Utils.h"
#include <systemc.h>

SC_MODULE(Acknowledge) {
public:
  sc_in_clk clock;
  sc_in<bool> reset;

  sc_vector<sc_in<Ack>> req_rx;
  sc_vector<sc_out<bool>> ack_tx;

  SC_CTOR(Acknowledge) {
    int num_tiles = GlobalParams::mesh_dim_x * GlobalParams::mesh_dim_y;
    req_rx.init(num_tiles);
    ack_tx.init(num_tiles);

    SC_THREAD(main_process);
    sensitive << clock.pos();
    async_reset_signal_is(reset, true);
  }

private:
  void main_process();
  void send_ack(const Ack &ack);
};

#endif // ACKNOWLEDGE_H
