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
    vector< Payload > generatePayloads(const Packet &packet);
    bool predictPayloadsOver(const vector < Flit > &flits, vector< Payload > &received, vector< Payload > &payloads);
    bool verifyPayloads(const vector < Payload > decoded, const vector < Payload > predicted);

    void onDecodeFailure() { _decodeCount++; _failureCount++; }
    void onDecodeSuccess(bool withoutError) { _decodeCount++; if (!withoutError) _errorCount++; }

    private:
    inline void hash_combine(size_t &seed, size_t value) {
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }
    unsigned int _decodeCount = 0, _failureCount = 0, _errorCount = 0;
};

#endif
