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
    double flit_keep_rate = 1.0 - GlobalParams::wired_flit_loss_rate;
    int wired_fl = 0, wired_be = 0, wireless_fl, wireless_be;

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
        wired_fl += flit.hop_no;
        wireless_fl += flit.hub_hop_no;
        wired_be += flit.hop_no * flit.payload.data.length();
        wireless_be += flit.hub_hop_no * flit.payload.data.length();
    }

    if (wired_fl > 0 && pseudo_prob_repeat(GlobalParams::wired_flit_loss_rate, wired_fl) > rand())
    {
        onDecodeFailure();
        return false;
    }

    if (wireless_fl > 0 && pseudo_prob_repeat(GlobalParams::wireless_flit_loss_rate, wireless_fl) > rand())
    {
        onDecodeFailure();
        return false;
    }
    
    if (wired_be > 0 && pseudo_prob_repeat(GlobalParams::wired_bit_error_rate, wired_be) > rand())
    {
        onDecodeSuccess(false);
        return true;
    }
    
    if (wireless_be > 0 && pseudo_prob_repeat(GlobalParams::wireless_bit_error_rate, wireless_be) > rand())
    {
        onDecodeSuccess(false);
        return true;
    }

    onDecodeSuccess(true);
    return true;
}
