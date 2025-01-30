#ifndef __NOXIMENCODINGMODEL_H__
#define __NOXIMENCODINGMODEL_H__

#include <vector>
#include <queue>
#include "../DataStructs.h"
#include "../Utils.h"

using namespace std;

struct Packet;
struct Flit;

class EncodingModel
{
    public:
    virtual ~EncodingModel() {};
    virtual bool encode(Packet &packet, queue < Flit > &sending_flits) = 0;
    virtual bool decode(vector < Flit > &received_flits, Packet &packet) = 0;
    int getDecodeCount() { return _decodeCount; }
    int getFailureCount() { return _failureCount; }
    int getErrorCount() { return _errorCount; }

    protected:
    // with calc
    vector< Payload > generatePayloads(const Packet &packet);
    bool predictPayloadsOver(const vector < Flit > &flits, vector< Payload > &received, vector< Payload > &payloads);
    bool verifyPayloads(const vector < Payload > decoded, const vector < Payload > predicted);

    // flip bit / loss flit
    static void simulate_hops(vector < Flit > &flits);

    // with virtual
    static double rand01() { return rand() / (RAND_MAX + 1.0); }
    static double pesudo_prob_poisson(int n, int k, double p);
    static double pseudo_prob_repeat(double p, int n);

    void onDecodeFailure() { _decodeCount++; _failureCount++; }
    void onDecodeSuccess(bool withoutError) { _decodeCount++; if (!withoutError) _errorCount++; }

    private:
    inline void hash_combine(size_t &seed, size_t value) {
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }
    unsigned int _decodeCount = 0, _failureCount = 0, _errorCount = 0;
};

#endif
