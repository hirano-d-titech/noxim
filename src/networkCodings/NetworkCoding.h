#ifndef __NOXIMNETWORKCODING_H__
#define __NOXIMNETWORKCODING_H__

#include "GlobalParams.h"

using namespace std;

class NetworkCoding {
    public:
    int getNetworkCodingType(std::string str){
        if (str == "XOR") {
            return NC_TYPE_XOR;
        }
        else if (str == "MATRIX") {
            return NC_TYPE_MATRIX;
        }
        return NC_TYPE_NONE;
    }
};

#endif
