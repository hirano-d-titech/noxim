#include "EncodingModel.h"

vector < Payload > EncodingModel::generatePayloads(const Packet &packet)
{
    vector < Payload > payloads;
    payloads.reserve(packet.flit_left);

    size_t base_hash = 0;
    hash_combine(base_hash, hash<int>()(packet.src_id));
    hash_combine(base_hash, hash<int>()(packet.dst_id));
    hash_combine(base_hash, hash<uint64_t>()(reinterpret_cast<const uint64_t&>(packet.timestamp)));

    for (int i = 0; i < packet.flit_left; ++i) {
        std::size_t seed = base_hash;
        hash_combine(seed, std::hash<int>()(i));
        uint32_t data_value = static_cast<uint32_t>(seed & 0xffffffff);

        payloads.push_back(Payload{data_value});
    }

    return payloads;
}

bool EncodingModel::predictPayloadsOver(const vector < Flit > &flits, vector< Payload > &received, vector< Payload > &predicted)
{
    size_t size = flits.size();
    if (size < 2) return false; // flit_buffer must contains 2 or more flits. (head and tail)
    Flit flit_head = flits[0];
    if (flit_head.flit_type != FLIT_TYPE_HEAD) return false; // top flit must be head typed.
    
    int src_id = flit_head.src_id;
    int dst_id = flit_head.dst_id;
    double timestamp = flit_head.timestamp;
    int length = flit_head.sequence_length;
    
    received.clear();
    received.reserve(size);
    received.push_back(flit_head.payload);

    for (size_t i = 1; i < size; i++)
    {
        Flit flit = flits[i];
        if (flit.src_id != src_id) return false; // all src_id should be same in flits.
        if (flit.dst_id != dst_id) return false; // all dst_id should be same in flits.
        if (flit.timestamp != timestamp) return false; // all timestamp should be same in flits.
        if (i == size-1)
            if (flit.flit_type != FLIT_TYPE_TAIL) return false; // last flit must be tail typed.
        else
            if (flit.flit_type != FLIT_TYPE_BODY) return false; // other flit must be body typed.

        received.push_back(flits[i].payload);
    }

    predicted.clear();
    predicted.reserve(length);
    
    size_t base_hash = 0;
    hash_combine(base_hash, hash<int>()(src_id));
    hash_combine(base_hash, hash<int>()(dst_id));
    hash_combine(base_hash, hash<uint64_t>()(reinterpret_cast<const uint64_t&>(timestamp)));

    for (int i = 0; i < length; ++i) {
        std::size_t seed = base_hash;
        hash_combine(seed, std::hash<int>()(i));
        uint32_t data_value = static_cast<uint32_t>(seed & 0xffffffff);

        predicted.push_back(Payload{data_value});
    }
    
    return true;
}

bool EncodingModel::verifyPayloads(const vector < Payload > decoded, const vector < Payload > predicted)
{
    size_t length = decoded.size();
    if (length > predicted.size()) return false;

    for (size_t i = 0; i < length; i++)
    {
        if (decoded[i] == predicted[i]) continue;
        return false;
    }

    return true;
}