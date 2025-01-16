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
    virtual bool encode(Packet &packet, queue < Flit > &sending_flits);
    virtual bool decode(vector < Flit > &received_flits, Packet &packet);
    int getErrorCount() { return _errorCount; }

    protected:
    vector< Payload > generatePayloads(const Packet &packet);
    bool predictPayloadsOver(const vector < Flit > &flits, vector< Payload > &received, vector< Payload > &payloads);
    bool verifyPayloads(const vector < Payload > decoded, const vector < Payload > predicted);

    void onDecodeFailure() { _decodeCount++; _failureCount++; }
    void onDecodeSuccess(bool withError) { _decodeCount++; if (withError) _errorCount++; }

    private:
    inline void hash_combine(size_t &seed, size_t value) {
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }
    int _decodeCount, _failureCount, _errorCount;
};

#endif
