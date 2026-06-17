/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the declaration of the router
 */

#ifndef __NOXIMROUTER_H__
#define __NOXIMROUTER_H__

#include <systemc.h>
#include <queue>
#include <utility>
#include "DataStructs.h"
#include "Buffer.h"
#include "Stats.h"
#include "GlobalRoutingTable.h"
#include "LocalRoutingTable.h"
#include "ReservationTable.h"
#include "Utils.h"
#include "DegradationMonitor.h"
#include "routingAlgorithms/RoutingAlgorithm.h"
#include "routingAlgorithms/RoutingAlgorithms.h"
#include "selectionStrategies/SelectionStrategy.h"
#include "selectionStrategies/SelectionStrategy.h"
#include "selectionStrategies/Selection_NOP.h"
#include "selectionStrategies/Selection_BUFFER_LEVEL.h"

using namespace std;

extern unsigned int drained_volume;



// Cluster boundary redundancy encoding state machine
enum ClusterEncState {
  CENC_IDLE,           // Waiting for cluster-crossing HEAD
  CENC_PROCESSING,     // Tracking BODY/TAIL flits through
  CENC_TAIL_PACKING,   // TAIL arrived, packing redundancy bits
  CENC_EXTRA_FLIT,     // Sending the extra TAIL flit
  CENC_RELEASE_WAIT    // Waiting to release port after extra flit
};

// Per-VC independent state for cluster encoding
struct ClusterEncContext {
  ClusterEncState state;
  ClusterEncodingType encoding_type;     // Encoding type to apply
  int redundancy_bits;                   // Number of redundancy bits to add
  int effective_bits_after;              // Effective bit length after adding redundancy
  int original_sequence_length;          // Original packet flit count
  bool needs_extra_flit;                 // Whether an extra flit is needed
  Flit extra_flit;                       // The extra TAIL flit to send
  int last_processed_cycle;              // Last cycle this context was processed (prevent dup)
  int output_port;                       // Target output port
  int input_port;                        // Source input port (for reservation release)
  ClusterEncodingMeta updated_enc_meta;  // Updated encoding metadata to propagate

  ClusterEncContext() : state(CENC_IDLE), encoding_type(CLUSTER_ENC_NONE),
    redundancy_bits(0), effective_bits_after(0), original_sequence_length(0),
    needs_extra_flit(false), last_processed_cycle(-1),
    output_port(NOT_VALID), input_port(NOT_VALID), updated_enc_meta() {}

  void reset() { *this = ClusterEncContext(); }
};

SC_MODULE(Router)
{
    friend class Selection_NOP;
    friend class Selection_BUFFER_LEVEL;

    // I/O Ports
    sc_in_clk clock;                      // The input clock for the router
    sc_in <bool> reset;                           // The reset signal for the router

    // number of ports: 4 mesh directions + local
    sc_in <Flit> flit_rx[DIRECTIONS + 1];    // The input channels 
    sc_in <bool> req_rx[DIRECTIONS + 1];    // The requests associated with the input channels
    sc_out <bool> ack_rx[DIRECTIONS + 1];    // The outgoing ack signals associated with the input channels
    sc_out <TBufferFullStatus> buffer_full_status_rx[DIRECTIONS+1];

    sc_out <Flit> flit_tx[DIRECTIONS + 1];   // The output channels
    sc_out <bool> req_tx[DIRECTIONS + 1];    // The requests associated with the output channels
    sc_in <bool> ack_tx[DIRECTIONS + 1];    // The outgoing ack signals associated with the output channels
    sc_in <TBufferFullStatus> buffer_full_status_tx[DIRECTIONS+1];

    sc_out <int> free_slots[DIRECTIONS + 1];
    sc_in <int> free_slots_neighbor[DIRECTIONS + 1];

    // Neighbor-on-Path related I/O
    sc_out < NoP_data > NoP_data_out[DIRECTIONS];
    sc_in < NoP_data > NoP_data_in[DIRECTIONS];

    // Registers

    int local_id;                    // Unique ID
    int my_cluster_id;               // Cluster ID of this router (2x2 clusters)
    int routing_type;                    // Type of routing algorithm
    int selection_type;
    BufferBank buffer[DIRECTIONS + 1];    // buffer[direction][virtual_channel] 
    bool current_level_rx[DIRECTIONS + 1];  // Current level for Alternating Bit Protocol (ABP)
    bool current_level_tx[DIRECTIONS + 1];  // Current level for Alternating Bit Protocol (ABP)
    Stats stats;                    // Statistics
    LocalRoutingTable routing_table;    // Routing table
    ReservationTable reservation_table;    // Switch reservation table
    unsigned long routed_flits;
    RoutingAlgorithm * routingAlgorithm; 
    SelectionStrategy * selectionStrategy; 

    DegradationMonitor degradation_monitor;
    bool active_in_current_cycle;
    std::queue<std::pair<Flit, int>> delay_buffer[DIRECTIONS + 1];

    // Functions

    void process();
    void rxProcess();    // The receiving process
    void txProcess();    // The transmitting process
    void perCycleUpdate();
    void configure(const int _id, const double _warm_up_time,
       const unsigned int _max_buffer_size,
       GlobalRoutingTable & grt);

    unsigned long getRoutedFlits();  // Returns the number of routed flits

    // Constructor

    SC_CTOR(Router) : local_id(0), my_cluster_id(0) {
      // Calculate my_cluster_id (assuming 2x2 clusters)
      int cx = local_id % GlobalParams::mesh_dim_x / 2;
      int cy = local_id / GlobalParams::mesh_dim_x / 2;
      int mesh_cx = (GlobalParams::mesh_dim_x + 1) / 2;
      my_cluster_id = cy * mesh_cx + cx;

      SC_METHOD(process);
      sensitive << reset;
      sensitive << clock.pos();

      SC_METHOD(perCycleUpdate);
      sensitive << reset;
      sensitive << clock.pos();

      routingAlgorithm = RoutingAlgorithms::get(GlobalParams::routing_algorithm);

      if (routingAlgorithm == 0)
      {
        cerr << " FATAL: invalid routing -routing " << GlobalParams::routing_algorithm << ", check with noxim -help" << endl;
        exit(-1);
      }

      selectionStrategy = SelectionStrategies::get(GlobalParams::selection_strategy);

      if (selectionStrategy == 0)
      {
        cerr << " FATAL: invalid selection strategy -sel " << GlobalParams::selection_strategy << ", check with noxim -help" << endl;
        exit(-1);
      }
    }

    // Cluster boundary encoding context per output-port per VC
    ClusterEncContext cluster_enc_ctx[DIRECTIONS + 1][MAX_VIRTUAL_CHANNELS];
    bool isClusterBoundaryCrossing(int output_port) const;
    void processClusterEncoding();
    void decideClusterEncodingType(int output_port, int vc_id, ClusterEncodingType &type, int &redundancy_bits, int effective_bits, int src_id);

  private:

    // performs actual routing + selection
    int route(const RouteData & route_data);

    // wrappers
    int selectionFunction(const vector <int> &directions,
        const RouteData & route_data);
    vector < int >routingFunction(const RouteData & route_data);

    NoP_data getCurrentNoPData();
    void NoP_report() const;
    int NoPScore(const NoP_data & nop_data, const vector <int> & nop_channels) const;
    vector<int> getNextHops(int src, int dst);
    int start_from_port;       // Port from which to start the reservation cycle
    int start_from_vc[DIRECTIONS+1]; // VC from which to start the reservation cycle for the specific port
  public:
    unsigned int local_drained;

    int reflexDirection(int direction) const;
    int getNeighborId(int _id, int direction) const;

    bool inCongestion();
    void ShowBuffersStats(std::ostream & out);
};

#endif
