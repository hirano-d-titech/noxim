#ifndef __NOXIMNC_XOR_H__
#define __NOXIMNC_XOR_H__

#include "../DataStructs.h"
#include "../Utils.h"
using namespace std;

struct Flit;

class NC_XOR {
    public:
    bool merge(Flit &src, const Flit &fm);
    bool mergeNew(const Flit &f1, const Flit &f2, Flit &fo);

    bool detachAll(Flit &src, const std::vector<Flit> &dst);
    bool detach(Flit &src, const Flit &dst);

    bool containsAll(const Flit &src, const std::vector<Flit> &dst);
    bool contains(const Flit &src, const Flit &dst);
    bool containsMeta(const Flit &src, const FlitMetadata &dst);

    static NC_XOR * getInstance();

    private:

    static NC_XOR * nc_XOR;
};

#endif
