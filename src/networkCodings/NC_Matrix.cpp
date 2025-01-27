#include "NC_Matrix.h"

NC_Matrix * NC_Matrix::nc_Matrix = 0;

NC_Matrix * NC_Matrix::getInstance() {
    if (nc_Matrix == 0)
    nc_Matrix = new NC_Matrix();

    return nc_Matrix;
}

bool NC_Matrix::mergeNew(const Flit &f1, const Flit &f2, vector<Flit> &merged)
{
    if (!Flit::mergeable(f1, f2)){
        return false;
    }

    merged.resize(2);

    Flit merge;

    // copy struct
    merged[0] = merge;
    merged[1] = merge;

    // possibillity overflow
    // G = | 1 1 |
    //     | 1 2 |
    // c1 = p1 + p2 (mod 2^32)
    // c2 = p1 (mod 2^32)
    merged[0].payload.data = f1.payload.data ^ f2.payload.data;
    merged[1].payload.data = f1.payload.data; // ^ (f2.payload.data << 1);

    // create merged history tree
    merged[0].import_tree(f1, f2);
    merged[1].import_tree(f1, f2);

    return true;
}

bool NC_Matrix::detach(const std::vector<Flit> &merged, Flit &f1, Flit &f2)
{
    if (merged.size() != 2) return false;

    auto m1 = merged[0];
    auto m2 = merged[1];

    if (!(m1.nc_meta == m2.nc_meta)) return false;

    // detach history tree
    f1.branch_tree(f1, f2);

    // possibillity overflow
    // G^-1 = |  2 -1 |
    //        | -1  1 |
    // f1 = e2 (mod 2^32)
    // f2 = e1 + e2 (mod 2^32)
    auto encoded1 = merged[0].payload.data;
    auto encoded2 = merged[1].payload.data;

    f1.payload.data = encoded2;
    f2.payload.data = encoded1 ^ encoded2;

    return true;
}
