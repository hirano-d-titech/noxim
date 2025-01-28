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
    FLIT_TYPE_HEAD, FLIT_TYPE_BODY, FLIT_TYPE_TAIL
};

enum NCState {
    NC_NORMAL, // not Network Coding encoded
    NC_ORIGIN, // has original destination
    NC_MERGED, // has other flit's destination merged by Network Coding
    NC_MULTIC, // needs to be multicasted towards children's destination
    NC_OPTION, // NC: other flit for decoding
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
    NCState nc_state;

    FlitMetadata(){}

    FlitMetadata(Packet packet){
        src_id = packet.src_id;
        dst_id = packet.dst_id;
        vc_id = packet.vc_id;
        timestamp = packet.timestamp;
        sequence_length = packet.size;
        use_low_voltage_path = packet.use_low_voltage_path;
        hub_relay_node = NOT_VALID;
        nc_state = NC_ORIGIN;
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
};

struct NCHistory {
    // make tree to store metadata merge history
    static constexpr int MAX_META = 4;

    // The ID indicates where the node is located in a particular tree structure.
    std::map<int /* id */, FlitMetadata /* node data */> metas;

    // depth represents the depth of the tree, but does not change the ID when it increases due to the structure of the tree.
    int depth;

    NCHistory(){
        depth = 0;
    }

    /**
     * returns whether merging is possible.
     */
    bool inline mergeable(const NCHistory& hist) const{
        return 2 + metas.size() + hist.metas.size() <= NCHistory::MAX_META;
    }

    /**
     * returns the number of elements at a particular tree depth
     */
    static inline int getTreeMax(int depth) {return (1 << depth) - 1;}

    // 2, 5, 6, 9, 12, 13, 14, 17, 20, 21, 24, 27, 28, 29, 30, ... => https://oeis.org/A055938
    static inline int nextTreeIdx(int id, int rep = 1) {
        static const int A055938[] = {2,5,6,9,12,13,14,17,20,21,24,27,28,29,30,33,36,37,40,43,44,45,48,51,52,55,58,59,60,61,62,65,68,69,72,75,76,77,80,83,84,87,90,91,92,93,96,99,100,103,106,107,108,111,114,115,118,121,122,123,124,125,126,129};
        switch (rep)
        {
        case 0:
            return id;
        case 1:
            return A055938[id];
        default:
            assert(rep > 1);
            auto ret = id;
            for (int i = 0; i < rep; i++)
            {
                ret = A055938[ret];
            }
            return ret;
        }
    }

    static inline int prevTreeIdx(int rev) {
        static const int A055938[] = {2,5,6,9,12,13,14,17,20,21,24,27,28,29,30,33,36,37,40,43,44,45,48,51,52,55,58,59,60,61,62,65,68,69,72,75,76,77,80,83,84,87,90,91,92,93,96,99,100,103,106,107,108,111,114,115,118,121,122,123,124,125,126,129};
        for(int i=0; i<64; i++){
            if(A055938[i] == rev){
                return i; // found
            }
        }
        return -1; // not found, return invalid value
    }

    // 2, 2, 6, 5, 5, 6, 14, 9, 9, 13, 12, 12, 13, 14, 17, 17, 21, 20, 20, 21, ... => not found in OEIS
    static inline int parentIdx(int id) {
        static const int PARENT[] = {2,2,6,5,5,6,14,9,9,13,12,12,13,14,17,17,21,20,20,21,29,24,24,28,27,27,28,29,30,62,33,33,37,36,36,37,45,40,40,44,43,43,44,45,48,48,52,51,51,52,60,55,55,59,58,58,59,60,61,62,126,65,65,69};
        return PARENT[id];
    }

    static inline pair<int, int> childrenIdx(int prt) {
        static const int PARENT[] = {2,2,6,5,5,6,14,9,9,13,12,12,13,14,17,17,21,20,20,21,29,24,24,28,27,27,28,29,30,62,33,33,37,36,36,37,45,40,40,44,43,43,44,45,48,48,52,51,51,52,60,55,55,59,58,58,59,60,61,62,126,65,65,69};
        pair<int, int> ret = {-1, -1};
        for(int i=0; i<64; i++){
            if(PARENT[i] == prt){
                if (ret.first == -1) {
                    ret.first = i;
                } else {
                    ret.second = i;
                    return ret;
                }
            }
        }
        return ret;
    }

    // 0, 1, 3, 4, 7, 8, 10, 11, 15, 16, 18, 19, 22, 23, 25, 26, 31, 32, ... => https://oeis.org/A005187
    static inline bool isGround(int id) {
        // static const int A005187[] = {0,1,3,4,7,8,10,11,15,16,18,19,22,23,25,26,31,32,34,35,38,39,41,42,46,47,49,50,53,54,56,57,63,64,66,67,70,71,73,74,78,79,81,82,85,86,88,89,94,95,97,98,101,102,104,105,109,110,112,113,116,117,119,120};
        static const bool LEAF[] = {1,1,0,1,1,0,0,1,1,0,1,1,0,0,0,1,1,0,1,1,0,0,1,1,0,1,1,0,0,0,0,1,1,0,1,1,0,0,1,1,0,1,1,0,0,0,1,1,0,1,1,0,0,1,1,0,1,1,0,0,0,0,0,1,1,0,1,1,0,0,1,1,0,1,1,0,0,0,1,1,0,1,1,0,0,1,1,0,1,1,0,0,0,0,1,1,0,1,1,0,0,1,1,0,1,1,0,0,0,1,1,0,1,1,0,0,1,1,0,1,1,0,0,0,0,0,0,1};
        if (id < 0 || id >= 128) return false;
        return LEAF[id];
    }

    bool isLeaf(int id)const {
        if (metas.find(id) == metas.end()) return false;
        if (isGround(id)) return true;
        auto children = childrenIdx(id);
        if (children.first < 0 || children.second < 0) assert(false);
        return metas.find(children.first) == metas.end() && metas.find(children.second) == metas.end();
    }

    void stepDepth(){
        std::map<int, FlitMetadata> other;
        for (auto &&pair : metas)
        {
            other.emplace(nextTreeIdx(pair.first), pair.second);
        }
        metas.swap(other);
        depth++;
    }

    bool inline hasAnyLeaf()const {
        for (auto &&pair : metas)
        {
            if (isGround(pair.first)) return true;
        }

        return false;
    }

    void cutDepth(){
        // if any leaf not found, history tree must be cut
        while (!hasAnyLeaf()) {
            std::map<int, FlitMetadata> other;
            bool error = false;
            for (auto &&meta : metas)
            {
                // find depth-1 id
                int after = prevTreeIdx(meta.first);
                if (after < 0) {
                    error = true;
                    break;
                }
                other.emplace(after, meta.second);
            }
            if (error) {
                std::cerr << "Error: cutDepth failed" << std::endl;
                break;
            }
            metas.swap(other);
            depth--;
        }
    }

    vector<pair<int, FlitMetadata>> getLeafNodes()const {
        vector<pair<int, FlitMetadata>> ret;
        for (auto &&pair : metas)
        {
            if (isLeaf(pair.first)) {
                ret.push_back({pair.first, pair.second});
            }
        }
        return ret;
    }

    void mergeHistory(FlitMetadata leftTop, NCHistory rightHist, FlitMetadata rightTop){
        assert(mergeable(rightHist));
        // make current tree depth large or equal to right tree.
        while (depth < rightHist.depth)
        {
            stepDepth();
        }
        depth++;
        // make current tree as left tree, and set leftTop as top metadata of left tree
        auto offset = getTreeMax(depth);
        metas.emplace(offset-1, leftTop);
        // create right tree with id offsetted, and deepened by diff
        auto diff = depth - rightHist.depth;
        for (auto &&meta : rightHist.metas)
        {
            metas.emplace(offset + nextTreeIdx(meta.first, diff), meta.second);
        }
        // make rightTop as top metadata of right tree
        metas.emplace(2*offset-1, rightTop);
    }

    bool removeHistory(int id, bool force = false){
        // remove node if any children doesnt exist
        if (isGround(id)) {
            metas.erase(id);
            return true;
        } else {
            auto children = childrenIdx(id);
            // impossible case
            if (children.first < 0 || children.second < 0) assert(false);
            if (force) {
                // if force true, remove all children recursively
                removeHistory(children.first, force);
                removeHistory(children.second, force);
                return true;
            } else {
                // if normal mode, remove only if both children doesnt exist
                if (metas.find(children.first) != metas.end() || metas.find(children.second) != metas.end()) {
                    return false;
                } else {
                    metas.erase(id);
                    return true;
                }
            }
        }
    }

    inline bool operator ==(const NCHistory & hist) {
        if (depth != hist.depth) return false;
        if (metas.size() != hist.metas.size()) return false;
        for (auto &&meta : metas)
        {
            if (hist.metas.find(meta.first) == hist.metas.end()) return false;
            if (!(hist.metas.at(meta.first) == meta.second)) return false;
        }
        return true;
    }
};

// Flit -- Flit definition
struct Flit {
    Payload payload;	// Optional payload

    FlitMetadata meta;
    NCHistory nc_meta;

    Flit(){}

    Flit(Packet packet, FlitType type){
        meta = {packet};
        meta.flit_type = type;
    }

    Flit(FlitMetadata metadata){
        meta = metadata;
    }

    int inline metaSize()const {
        return 1 + nc_meta.metas.size();
    }

    static bool mergeable(const Flit& f1, const Flit& f2){
        return f1.nc_meta.mergeable(f2.nc_meta);
    }

    bool import_tree(const Flit& f1, const Flit& f2){
        nc_meta = f1.nc_meta;
        nc_meta.mergeHistory(f1.meta, f2.nc_meta, f2.meta);
    }

    bool branch_tree(Flit &f1, Flit &f2){
        if (nc_meta.depth == 0) return false;
        if (nc_meta.metas.size() == 0) return false;

        auto tree = NCHistory::getTreeMax(nc_meta.depth);
        auto f1_it = nc_meta.metas.find(tree-1);
        auto f2_it = nc_meta.metas.find(2*tree-1);
        if (f1_it == nc_meta.metas.end() || f2_it == nc_meta.metas.end()) return false;

        for (auto &&meta : nc_meta.metas)
        {
            if (meta.first < tree) {
                if (meta.first == tree-1) {
                    f1.meta = meta.second;
                } else {
                    f1.nc_meta.metas.emplace(meta.first, meta.second);
                }
            } else {
                if (meta.first == 2*tree-1) {
                    f2.meta = meta.second;
                } else {
                    f2.nc_meta.metas.emplace(meta.first-tree, meta.second);
                }
            }
        }

        f1.nc_meta.depth = nc_meta.depth-1;
        f2.nc_meta.depth = nc_meta.depth-1;
        f1.nc_meta.cutDepth();
        f2.nc_meta.cutDepth();
        
        return true;
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
