#include "NC_XOR.h"

bool NC_XOR::merge(const Flit &f1, const Flit &f2, Flit &fo)
{
    Flit merged;
    merged.payload = Payload{f1.payload.data ^ f2.payload.data};
}