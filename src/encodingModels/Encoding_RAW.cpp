#include "Encoding_RAW.h"

EncodingModelsRegister Encoding_RAW::encodingModelsRegister("RAW", getInstance());

Encoding_RAW * Encoding_RAW::encoding_RAW = 0;

Encoding_RAW * Encoding_RAW::getInstance() {
    if (encoding_RAW == 0)
        encoding_RAW = new Encoding_RAW();
    return encoding_RAW;
}

bool Encoding_RAW::encode(Packet &packet, queue < Flit > &sending_flits) {
    for (size_t i = 0; i < packet.size; i++)
    {
        Flit flit(packet);
        flit.sequence_no = i;
        flit.hub_relay_node = NOT_VALID;

        if (i == 0)
            flit.flit_type = FLIT_TYPE_HEAD;
        else if (i == packet.size-1)
            flit.flit_type = FLIT_TYPE_TAIL;
        else
            flit.flit_type = FLIT_TYPE_BODY;

        sending_flits.push(flit);
    }

    return true;
}

bool Encoding_RAW::decode(vector < Flit > &received_flits, Packet &packet) {
    packet = reconstructPacket(received_flits);
    if (packet.size != (int)received_flits.size())
    {
        onDecodeFailure();
        return false;
    }

    for (auto &&flit : received_flits)
    {
        if (pseudo_prob_repeat(GlobalParams::wired_flit_loss_rate, flit.hop_no) > rand01())
        {
            onDecodeFailure();
            return false;
        }

        if (pseudo_prob_repeat(GlobalParams::wireless_flit_loss_rate, flit.hub_hop_no) > rand01())
        {
            onDecodeFailure();
            return false;
        }

        auto len = flit.payload.data.length();
        if (pseudo_prob_repeat(GlobalParams::wired_bit_error_rate, flit.hop_no * len) > rand01())
        {
            onDecodeSuccess(false);
            return true;
        }

        if (pseudo_prob_repeat(GlobalParams::wireless_bit_error_rate, flit.hub_hop_no * len) > rand01())
        {
            onDecodeSuccess(false);
            return true;
        }
    }

    onDecodeSuccess(true);
    return true;
}
