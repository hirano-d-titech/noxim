/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the declaration of the top-level of Noxim
 */

#ifndef _DATASTRUCS_H__
#define _DATASTRUCS_H__

#include <systemc.h>
#include "GlobalParams.h"

// RouteMetadata -- metadata to perform custom routing
struct RouteMetadata {
  std::vector<int> custom_data;

  inline bool operator ==(const RouteMetadata & other) const {
    return (custom_data == other.custom_data);
  }
};

// Coord -- XY coordinates type of the Tile inside the Mesh
class Coord {
  public:
    int x;      // X coordinate
    int y;      // Y coordinate

    inline bool operator ==(const Coord & coord) const {
  return (coord.x == x && coord.y == y);
}};

// FlitType -- Flit type enumeration
enum FlitType {
  FLIT_TYPE_HEAD, FLIT_TYPE_BODY, FLIT_TYPE_TAIL
};

// Payload -- Payload definition
struct Payload {
  sc_uint<32> data;  // Bus for the data to be exchanged

  inline bool operator ==(const Payload & payload) const {
  return (payload.data == data);
}};

// Packet -- Packet definition
struct Packet {
  int src_id;
  int dst_id;
  int vc_id;
  double timestamp;    // SC timestamp at packet generation
  int size;
  int flit_left;    // Number of remaining flits inside the packet
  bool use_low_voltage_path;
  int packet_id; // パケットを識別する一意なID。デフォルトは NOT_VALID
  RouteMetadata route_metadata;

  // Constructors
  Packet() : packet_id(NOT_VALID) { }

  Packet(const int s, const int d, const int vc, const double ts, const int sz) {
    make(s, d, vc, ts, sz);
  }

  void make(const int s, const int d, const int vc, const double ts, const int sz) {
    src_id = s;
    dst_id = d;
    vc_id = vc;
    timestamp = ts;
    size = sz;
    flit_left = sz;
    use_low_voltage_path = false;
    packet_id = NOT_VALID;
  }

  Packet reverse()
  {
    Packet p = Packet{dst_id, src_id, vc_id, sc_time_stamp().to_double() / GlobalParams::clock_period_ps, size};
    p.packet_id = packet_id;
    p.route_metadata = route_metadata;
    return p;
  }
};

// Ack -- Ack signal definition
struct Ack {
    int src_id;
    int dst_id;
    int packet_id;
    bool is_nack;

    Ack() : src_id(NOT_VALID), dst_id(NOT_VALID), packet_id(NOT_VALID), is_nack(false) {}
    Ack(int s, int d, int pid, bool nack = false) : src_id(s), dst_id(d), packet_id(pid), is_nack(nack) {}

    inline bool operator ==(const Ack & ack) const {
      return (ack.src_id == src_id && ack.dst_id == dst_id && 
              ack.packet_id == packet_id && ack.is_nack == is_nack);
    }

    inline bool isValid() const {
      return (src_id != NOT_VALID && dst_id != NOT_VALID && packet_id != NOT_VALID);
    }
};

struct Flit; // Forward declaration

// RouteData -- data required to perform routing
struct RouteData {
  int current_id;
  int src_id;
  int dst_id;
  int dir_in;      // direction from which the packet comes from
  int vc_id;
  Flit * flit;

  RouteData() : current_id(NOT_VALID), src_id(NOT_VALID), dst_id(NOT_VALID), dir_in(NOT_VALID), vc_id(NOT_VALID), flit(nullptr) {}
};

struct ChannelStatus {
  int free_slots;    // occupied buffer slots
  bool available;    // 
  inline bool operator ==(const ChannelStatus & bs) const {
    return (free_slots == bs.free_slots && available == bs.available);
  };
};

// NoP_data -- NoP Data definition
struct NoP_data {
  int sender_id;
  ChannelStatus channel_status_neighbor[DIRECTIONS];

  inline bool operator ==(const NoP_data & nop_data) const {
  return (sender_id == nop_data.sender_id &&
    nop_data.channel_status_neighbor[0] ==
    channel_status_neighbor[0]
    && nop_data.channel_status_neighbor[1] ==
    channel_status_neighbor[1]
    && nop_data.channel_status_neighbor[2] ==
    channel_status_neighbor[2]
    && nop_data.channel_status_neighbor[3] ==
    channel_status_neighbor[3]);
  };
};

struct TBufferFullStatus {
  TBufferFullStatus()
  {
    for (int i=0;i<MAX_VIRTUAL_CHANNELS;i++)
      mask[i] = false;
  };

  inline bool operator ==(const TBufferFullStatus & bfs) const {
    for (int i=0;i<MAX_VIRTUAL_CHANNELS;i++)
      if (mask[i] != bfs.mask[i]) return false;
    return true;
  };

  bool mask[MAX_VIRTUAL_CHANNELS];
};

// Flit -- Flit definition
struct Flit {
  int src_id;
  int dst_id;
  int vc_id; // Virtual Channel
  FlitType flit_type;  // The flit type (FLIT_TYPE_HEAD, FLIT_TYPE_BODY, FLIT_TYPE_TAIL)
  int sequence_no;    // The sequence number of the flit inside the packet
  int sequence_length;
  Payload payload;  // Optional payload
  double timestamp;    // Unix timestamp at packet generation
  int hop_no;      // Current number of hops from source to destination
  int hub_hop_no;     // Current number of passed wireless-hops
  bool use_low_voltage_path;
  int packet_id; // 対応するパケットの一意ID
  RouteMetadata route_metadata;

  Flit() : packet_id(NOT_VALID) {}

  Flit(Packet packet){
    src_id = packet.src_id;
    dst_id = packet.dst_id;
    vc_id = packet.vc_id;
    timestamp = packet.timestamp;
    sequence_length = packet.size;
    hop_no = 0;
    hub_hop_no = 0;
    use_low_voltage_path = packet.use_low_voltage_path;
    packet_id = packet.packet_id;
    route_metadata = packet.route_metadata;
  }

  inline bool operator ==(const Flit & flit) const {
    return (flit.src_id == src_id && flit.dst_id == dst_id
      && flit.flit_type == flit_type
      && flit.vc_id == vc_id
      && flit.sequence_no == sequence_no
      && flit.sequence_length == sequence_length
      && flit.payload == payload && flit.timestamp == timestamp
      && flit.hop_no == hop_no
      && flit.use_low_voltage_path == use_low_voltage_path
      && flit.packet_id == packet_id
      && flit.route_metadata == route_metadata);
  }
};

#endif
