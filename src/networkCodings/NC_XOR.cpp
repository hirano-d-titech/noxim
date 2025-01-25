#include "NC_XOR.h"

NC_XOR * NC_XOR::nc_XOR = 0;

NC_XOR * NC_XOR::getInstance() {
    if (nc_XOR == 0)
    nc_XOR = new NC_XOR();

    return nc_XOR;
}

bool NC_XOR::merge(Flit &src, const Flit &fm)
{
    if (src.nc_meta.size() + fm.nc_meta.size() + 1 > Flit::MAX_NC_META){
        return false;
    }

    src.payload.data ^= fm.payload.data;

    src.merge_nc(fm.nc_meta);
    src.merge_nc(fm.meta);
    return true;
}

bool NC_XOR::mergeNew(const Flit &f1, const Flit &f2, Flit &fo)
{
    if (f1.nc_meta.size() + f2.nc_meta.size() + 2 > Flit::MAX_NC_META){
        return false;
    }

    fo.payload = Payload{f1.payload.data ^ f2.payload.data};

    fo.merge_nc(f1.nc_meta);
    fo.merge_nc(f2.nc_meta);
    fo.merge_nc(f1.meta);
    fo.merge_nc(f2.meta);
    return true;
}

bool NC_XOR::detachAll(Flit &src, const std::vector<Flit> &dst)
{
    if (!containsAll(src, dst)) return false;

    for (auto &&flit : dst)
    {
        detach(src, flit);
    }
    return true;
}

bool NC_XOR::detach(Flit &src, const Flit &dst)
{
    for (size_t i = 0; i < src.nc_meta.size(); i++)
    {
        if (src.nc_meta[i] == dst.meta) {
            src.payload = {src.payload.data ^ dst.payload.data};
            src.eraceFindIf(dst.meta);
            return true;
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
    for (size_t i = 0; i < src.nc_meta.size(); i++)
    {
        if (src.nc_meta[i] == dst.meta) return true;
    }
    return false;
}

bool NC_XOR::containsMeta(const Flit &src, const FlitMetadata &dst)
{
    for (size_t i = 0; i < src.nc_meta.size(); i++)
    {
        if (src.nc_meta[i] == dst) return true;
    }
    return false;
}
