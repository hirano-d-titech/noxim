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

Packet EncodingModel::reconstructPacket(const vector < Flit > &flits)
{
    return Packet{flits[0].src_id, flits[0].dst_id, flits[0].vc_id, flits[0].timestamp, flits[0].sequence_length};
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
        if (i == size-1) {
            if (flit.flit_type != FLIT_TYPE_TAIL) return false; // last flit must be tail typed.
        } else {
            if (flit.flit_type != FLIT_TYPE_BODY) return false; // other flit must be body typed.
        }
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

void EncodingModel::simulate_hops(vector < Flit > &flits)
{
    vector < Flit > after;
    for (auto &&flit : flits)
    {
        bool lost = false;
        for (int i = 0; i < flit.hop_no; i++)
        {
            if (rand01() < GlobalParams::wired_flit_loss_rate) {
                lost = true;
                break;
            }
            if (rand01() < GlobalParams::wired_bit_error_rate) {
                flit.payload.data ^= (1 << (rand() % 32));
            }
        }
        if (!lost) after.push_back(flit);
    }
    flits.swap(after);

    after.clear();
    for (auto &&flit : flits)
    {
        bool lost = false;
        for (int i = 0; i < flit.hub_hop_no; i++)
        {
            if (rand01() < GlobalParams::wireless_flit_loss_rate) {
                lost = true;
                break;
            }
            if (rand01() < GlobalParams::wireless_bit_error_rate) {
                flit.payload.data ^= (1 << (rand() % 32));
            }
        }
        if (!lost) after.push_back(flit);
    }
    flits.swap(after);
}

double EncodingModel::pesudo_prob_poisson(int n, int k, double p){
    assert(p >= 0.0 && p <= 1.0);
    assert(n >= 0 && k >= 0);

    if (n < k) return 0;

    double lambda = n * p;
    if(lambda == 0.0) return (k <= 0) ? 1.0 : 0.0;

    double sum = 0.0;
    double term = exp(-lambda);
    for(int i = 0; i < k; ++i){
        sum += term;
        term *= lambda / (i + 1);
    }

    return 1.0 - sum;
}

double EncodingModel::pseudo_prob_repeat(double p, int n){
    assert(p >= 0.0 && p <= 1.0);
    assert(n >= 0);

    if (p == 0.0) return 0.0;
    if (n == 0) return 0.0;
    if (p == 1.0) return 1.0;

    double lambda = static_cast<double>(n) * p;

    // if lambda is very small, it can be approximated
    if (lambda < 1e-7) return lambda;

    return -std::expm1(-lambda);
}