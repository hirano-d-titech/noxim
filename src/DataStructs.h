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

// Coord -- XY coordinates type of the Tile inside the Mesh
class Coord {
  public:
    int x;			// X coordinate
    int y;			// Y coordinate

    inline bool operator ==(const Coord & coord) const {
	return (coord.x == x && coord.y == y);
}};

// FlitType -- Flit type enumeration
enum FlitType {
    FLIT_TYPE_HEAD, FLIT_TYPE_BODY, FLIT_TYPE_TAIL, FLIT_TYPE_TRAIL
};

enum NCState {
    /**
     * NC_MULTIC: needs to be multicasted towards children's destination. (stands for Multicast)
     * If any flit metadata of nc_meta has NC_MULTIC, the parent metadata must have it.
     * In all router at reservation phase, it MUST search for child routes that are not NC_OPTION.
     * If one of the child wants to route other side of parent, flit must be multicasted.
     * On multicasting, some of the ncstate of flit in nc_meta should be changed.
     */
    NC_MULTIC,

    /**
     * NC_NORMAL: the most ordinary state of flit. just the data, or not network coding encoded.
     * Flit in this state should never have child meta.
     */
    NC_NORMAL,

    /**
     * NC_MERGED: flit does not have multiple destinations in its children, but is encoded
     * Flit in this state should never be leaf meta.
     */
    NC_MERGED,

    /**
     * NC_OPTION: this metadata is stored for decoding and will be removed at decoding.
     * Flit in this state should never have child meta without NC_OPTION.
     */
    NC_OPTION,
};

// Payload -- Payload definition
struct Payload {
    sc_uint<32> data;	// Bus for the data to be exchanged

    inline bool operator ==(const Payload & payload) const {
	return (payload.data == data);
}};

// Packet -- Packet definition
struct Packet {
    int src_id;
    int dst_id;
    int vc_id;
    double timestamp;		// SC timestamp at packet generation
    int size;
    int flit_left;		// Number of remaining flits inside the packet
    bool use_low_voltage_path;

    // Constructors
    Packet() { }

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
    }
};

// RouteData -- data required to perform routing
struct RouteData {
    int current_id;
    int src_id;
    int dst_id;
    int dir_in;			// direction from which the packet comes from
    int vc_id;
};

struct ChannelStatus {
    int free_slots;		// occupied buffer slots
    bool available;		// 
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

struct TBufferCapStatus {
    TBufferCapStatus()
    {
	for (int i=0;i<MAX_VIRTUAL_CHANNELS;i++)
	    mask[i] = GlobalParams::buffer_depth;
    };
    inline bool operator ==(const TBufferCapStatus & bfs) const {
	for (int i=0;i<MAX_VIRTUAL_CHANNELS;i++)
	    if (mask[i] != bfs.mask[i]) return false;
	return true;
    };
   
    int mask[MAX_VIRTUAL_CHANNELS];
};

struct FlitMetadata {
    int src_id;
    int dst_id;
    int vc_id; // Virtual Channel
    FlitType flit_type;	// The flit type (FLIT_TYPE_HEAD, FLIT_TYPE_BODY, FLIT_TYPE_TAIL)
    int sequence_no;		// The sequence number of the flit inside the packet
    int sequence_length;
    double timestamp;		// Unix timestamp at packet generation
    int hop_no;		// Current number of hops from source to destination
    int hub_hop_no;     // Current number of passed wireless-hops
    bool use_low_voltage_path;

    int hub_relay_node;

    FlitMetadata(){}

    FlitMetadata(Packet packet){
        src_id = packet.src_id;
        dst_id = packet.dst_id;
        vc_id = packet.vc_id;
        timestamp = packet.timestamp;
        sequence_length = packet.size;
        hop_no = 0;
        hub_hop_no = 0;
        use_low_voltage_path = packet.use_low_voltage_path;
        hub_relay_node = NOT_VALID;
    }

    inline bool operator ==(const FlitMetadata & meta) const {
	return (meta.src_id == src_id && meta.dst_id == dst_id
		&& meta.flit_type == flit_type
		&& meta.vc_id == vc_id
		&& meta.sequence_no == sequence_no
		&& meta.sequence_length == sequence_length
		&& meta.timestamp == timestamp
		&& meta.use_low_voltage_path == use_low_voltage_path);
    }

    inline bool like(const FlitMetadata & meta) const {
	return (meta.src_id == src_id && meta.dst_id == dst_id
		&& meta.vc_id == vc_id
		&& meta.sequence_length == sequence_length
		&& meta.timestamp == timestamp
		&& meta.use_low_voltage_path == use_low_voltage_path);
    }
};

// Flit -- Flit definition
struct Flit {
    Payload payload;	// Optional payload

    FlitMetadata meta, merged;
    NCState nc_state = NC_NORMAL;

    Flit(){}

    Flit(Packet packet, FlitType type){
        meta = {packet};
        meta.flit_type = type;
    }

    Flit(FlitMetadata metadata){
        meta = metadata;
    }

    void swapMeta()
    {
        auto temp = meta;
        meta = merged;
        merged = temp;
    }

    inline bool operator ==(const Flit & flit) const {
	return (flit.meta == meta && flit.payload == payload);
}};

typedef struct 
{
    string label;
    double value;
} PowerBreakdownEntry;


enum
{
    BUFFER_PUSH_PWR_D,
    BUFFER_POP_PWR_D,
    BUFFER_FRONT_PWR_D,
    BUFFER_TO_TILE_PUSH_PWR_D,
    BUFFER_TO_TILE_POP_PWR_D,
    BUFFER_TO_TILE_FRONT_PWR_D,
    BUFFER_FROM_TILE_PUSH_PWR_D,
    BUFFER_FROM_TILE_POP_PWR_D,
    BUFFER_FROM_TILE_FRONT_PWR_D,
    ANTENNA_BUFFER_PUSH_PWR_D,
    ANTENNA_BUFFER_POP_PWR_D,
    ANTENNA_BUFFER_FRONT_PWR_D,
    ROUTING_PWR_D,
    SELECTION_PWR_D,
    CROSSBAR_PWR_D,
    LINK_R2R_PWR_D,
    LINK_R2H_PWR_D,
    NI_PWR_D,
    WIRELESS_TX,
    WIRELESS_DYNAMIC_RX_PWR,
    WIRELESS_SNOOPING,
    NO_BREAKDOWN_ENTRIES_D
};

enum
{
    TRANSCEIVER_RX_PWR_BIASING,
    TRANSCEIVER_TX_PWR_BIASING,
    BUFFER_ROUTER_PWR_S,
    BUFFER_TO_TILE_PWR_S,
    BUFFER_FROM_TILE_PWR_S,
    ANTENNA_BUFFER_PWR_S,
    LINK_R2H_PWR_S,
    ROUTING_PWR_S,
    SELECTION_PWR_S,
    CROSSBAR_PWR_S,
    NI_PWR_S,
    TRANSCEIVER_RX_PWR_S,
    TRANSCEIVER_TX_PWR_S,
    NO_BREAKDOWN_ENTRIES_S
};

typedef struct
{
    int size;
    PowerBreakdownEntry breakdown[NO_BREAKDOWN_ENTRIES_D+NO_BREAKDOWN_ENTRIES_S];
} PowerBreakdown;


#endif
