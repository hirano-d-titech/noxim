#ifndef __NOXIMNetworkCoding_H__
#define __NOXIMNetworkCoding_H__

#include "DataStructs.h"
#include "Utils.h"
using namespace std;

struct Flit;

class NetworkCoding {
    public:
    static bool merge(Flit &fo, const Flit &sub);

    static bool detach(Flit &fo, const Flit &sub);

    static bool contains(const Flit &src, const Flit &dst);
};

#endif
