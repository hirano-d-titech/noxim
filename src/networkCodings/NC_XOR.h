#ifndef __NOXIMNC_XOR_H__
#define __NOXIMNC_XOR_H__

#include "../DataStructs.h"
#include "../Utils.h"
using namespace std;

struct Flit;

class NC_XOR {
    public:
    bool merge(const Flit &f1, const Flit &f2, Flit &fo);
    bool findFlit(const Flit &f);

    private:
    
};

#endif
