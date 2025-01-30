#ifndef __NOXIMENCODING_HAMMING_H__
#define __NOXIMENCODING_HAMMING_H__

#include <systemc.h>
#include "EncodingModel.h"
#include "EncodingModels.h"

using namespace std;

class Encoding_HAMMING : public EncodingModel {
    public:
        bool encode(Packet &packet, queue < Flit > &sending_flits);
        bool decode(vector < Flit > &received_flits, Packet &packet);

        static Encoding_HAMMING * getInstance();

    private:
        Encoding_HAMMING(){};
        ~Encoding_HAMMING(){};

        static Encoding_HAMMING * encoding_HAMMING;
        static EncodingModelsRegister encodingModelsRegister;
};

#endif
