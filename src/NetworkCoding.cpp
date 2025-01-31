#include "NetworkCoding.h"

bool NetworkCoding::merge(Flit &fo, const Flit &sub)
{
    if (fo.nc_state != NC_NORMAL || sub.nc_state != NC_NORMAL) return false;

    fo.payload = Payload{fo.payload.data ^ sub.payload.data};
    fo.merged = sub.meta;

    return true;
}

bool NetworkCoding::detach(Flit &fo, const Flit &sub)
{
    if (fo.nc_state != NC_MERGED || sub.nc_state != NC_OPTION) return false;
    if (!fo.merged.like(sub.meta)) return false;

    fo.payload = Payload{fo.payload.data ^ sub.payload.data};
    fo.merged = FlitMetadata{};

    return true;
}

bool NetworkCoding::contains(const Flit &src, const Flit &dst)
{
    if (dst.nc_state != NC_OPTION) return false;
    return src.merged.like(dst.meta);
}
