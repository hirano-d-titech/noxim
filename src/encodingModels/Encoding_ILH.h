#ifndef __NOXIMEncoding_ILH_H__
#define __NOXIMEncoding_ILH_H__

#include <systemc.h>
#include "EncodingModel.h"
#include "EncodingModels.h"

using namespace std;

class Encoding_ILH : public EncodingModel {
    public:
        bool encode(Packet &packet, queue < Flit > &sending_flits);
        bool decode(vector < Flit > &received_flits, Packet &packet);

        static Encoding_ILH * getInstance();

    private:
        Encoding_ILH(){};
        ~Encoding_ILH(){};

        static Encoding_ILH * encoding_ILH;
        static EncodingModelsRegister encodingModelsRegister;
};

#endif
