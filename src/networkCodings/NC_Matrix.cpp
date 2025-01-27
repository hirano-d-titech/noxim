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
    
    // 構造体をコピー
    merged[0] = merge;
    merged[1] = merge;

    // c1 = p1 + p2 (mod 2^32)
    // c2 = p1 + 2*p2 (mod 2^32)
    sc_uint<32> c1 = f1.payload.data + f2.payload.data;
    sc_uint<32> c2 = f1.payload.data + (f2.payload.data << 1);

    // 生成した2つのFlitにpayloadをセット
    merged[0].payload.data = c1;
    merged[1].payload.data = c2;
    
    // 木構造を導入
    merged[0].import_tree(f1, f2);
    merged[1].import_tree(f1, f2);

    return true;
}

bool NC_Matrix::detach(const std::vector<Flit> &merged, Flit &f1, Flit &f2)
{
    // TODO: this is auto-completed code, check later
    if (merged.size() != 2) return false;

    if (!Flit::mergeable(f1, f2)) return false;

    // 木構造を分解
    f1.branch_tree(f1, f2);

    return true;
}