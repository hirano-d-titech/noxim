#ifndef __NOXIMENCODING_REPEAT_H__
#define __NOXIMENCODING_REPEAT_H__

#include "EncodingModel.h"
#include "EncodingModels.h"

using namespace std;

class Encoding_REPEAT : public EncodingModel {
    public:
        bool encode(Packet &packet, queue < Flit > &sending_flits)override;
        bool decode(vector < Flit > &received_flits, Packet &packet)override;

        static Encoding_REPEAT * getInstance();

    private:
        Encoding_REPEAT(){};
        ~Encoding_REPEAT(){};

        constexpr static int REPETITION = 3;

        static Encoding_REPEAT * encoding_REPEAT;
        static EncodingModelsRegister encodingModelsRegister;
};

#endif
