#ifndef __NOXIMENCODING_RAW_H__
#define __NOXIMENCODING_RAW_H__

#include "EncodingModel.h"
#include "EncodingModels.h"

using namespace std;

class Encoding_RAW : public EncodingModel {
    public:
        bool encode(Packet &packet, queue < Flit > &sending_flits);
        bool decode(vector < Flit > &received_flits, Packet &packet);

        static Encoding_RAW * getInstance();

    private:
        Encoding_RAW(){};
        ~Encoding_RAW(){};

        static Encoding_RAW * encoding_RAW;
        static EncodingModelsRegister encodingModelsRegister;
};

#endif
