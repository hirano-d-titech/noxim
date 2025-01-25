#ifndef __NOXIMNC_MATRIX_H__
#define __NOXIMNC_MATRIX_H__

#include "../DataStructs.h"
#include "../Utils.h"
#include <vector>

using namespace std;

struct Flit;

class NC_Matrix {
    public:
    bool mergeNew(const Flit &f1, const Flit &f2, Flit &fo);

    static NC_Matrix * getInstance();

    private:

    static NC_Matrix * nc_Matrix;
};

#endif
