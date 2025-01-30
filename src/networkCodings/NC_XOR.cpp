#include "NC_XOR.h"

NC_XOR * NC_XOR::nc_XOR = 0;

NC_XOR * NC_XOR::getInstance() {
    if (nc_XOR == 0)
    nc_XOR = new NC_XOR();

    return nc_XOR;
}

bool NC_XOR::mergeNew(const Flit &f1, const Flit &f2, Flit &fo)
{
    if (!Flit::mergeable(f1, f2)){
        return false;
    }

    fo.payload = Payload{f1.payload.data ^ f2.payload.data};
    fo.import_tree(f1, f2);

    return true;
}

bool NC_XOR::detachAll(Flit &src, const std::vector<Flit> &dst)
{
    if (!containsAll(src, dst)) return false;
    for (auto &&pair : src.nc_meta.getLeafMetas())
    {
        bool found = false;
        for (auto &&flit : dst)
        {
            if (pair.second == flit.meta) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    for (auto &&flit : dst)
    {
        detach(src, flit);
    }
    return true;
}

bool NC_XOR::detach(Flit &src, const Flit &dst)
{
    for (auto &&pair : src.nc_meta.getLeafMetas())
    {
        if (pair.second == dst.meta) {
            src.payload = {src.payload.data ^ dst.payload.data};
            return src.nc_meta.removeHistory(pair.first);
        }
    }
    return false;
}

bool NC_XOR::containsAll(const Flit &src, const std::vector<Flit> &dst)
{
    for (auto &&flit : dst)
    {
        if (!contains(src, flit)) return false;
    }
    return true;
}

bool NC_XOR::contains(const Flit &src, const Flit &dst)
{
    return containsMeta(src, dst.meta);
}

bool NC_XOR::containsMeta(const Flit &src, const FlitMetadata &dst)
{
    for (auto &&pair : src.nc_meta.metas)
    {
        if (pair.second == dst) return true;
    }
    return false;
}
