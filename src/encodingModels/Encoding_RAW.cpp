#include "Encoding_RAW.h"

EncodingModelsRegister Encoding_RAW::encodingModelsRegister("RAW", getInstance());

Encoding_RAW * Encoding_RAW::encoding_RAW = 0;

Encoding_RAW * Encoding_RAW::getInstance() {
    if (encoding_RAW == 0)
        encoding_RAW = new Encoding_RAW();
    return encoding_RAW;
}

bool Encoding_RAW::encode(Packet &packet, queue < Flit > &sending_flits) {
    auto payloads = generatePayloads(packet);
    size_t size = payloads.size();

    for (size_t i = 0; i < size; i++)
    {
        Flit flit(packet);
        flit.sequence_no = i;
        flit.payload = payloads[i];

        flit.hub_relay_node = NOT_VALID;

        if (i == 0)
            flit.flit_type = FLIT_TYPE_HEAD;
        else if (i == size-1)
            flit.flit_type = FLIT_TYPE_TAIL;
        else
            flit.flit_type = FLIT_TYPE_BODY;

        sending_flits.push(flit);
    }

    return true;
}

bool Encoding_RAW::decode(vector < Flit > &received_flits, Packet &packet) {
    double flit_keep_rate = 1.0 - GlobalParams::wired_flit_loss_rate;
    map<int, int> bit_error_map;

    for (auto &&flit : received_flits)
    {
        // calculate flit loss rate
        if (pow(flit_keep_rate, flit.hop_no) < rand() / (RAND_MAX + 1.0))
        {
            // if at least one flit loss, it cant be decode.
            onDecodeFailure();
            return false;
        }

        // counting flipping-bit chance
        if (bit_error_map.find(flit.hop_no) != bit_error_map.end())
        {
            bit_error_map.at(flit.hop_no) += flit.payload.data.length();
        }
        else
        {
            bit_error_map.emplace(flit.hop_no, flit.payload.data.length());
        }
    }

    for (auto &&pair : bit_error_map)
    {
        
    }

    onDecodeSuccess(true);
    return true;
}
