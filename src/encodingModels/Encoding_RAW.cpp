#include "Encoding_RAW.h"

EncodingModelsRegister Encoding_RAW::encodingModelsRegister("RAW", getInstance());

Encoding_RAW * Encoding_RAW::encoding_RAW = 0;

Encoding_RAW * Encoding_RAW::getInstance() {
    if (encoding_RAW == 0)
        encoding_RAW = new Encoding_RAW();
    return encoding_RAW;
}

bool Encoding_RAW::encode(Packet &packet, queue < Flit > &sending_flits) {
    for (int i = 0; i < packet.size; i++)
    {
        auto type = i == 0 ? FLIT_TYPE_HEAD :
                    i == packet.size-1 ? FLIT_TYPE_TAIL :
                    FLIT_TYPE_BODY;

        Flit flit(packet, type);
        flit.meta.sequence_no = i;
        flit.meta.hub_relay_node = NOT_VALID;

        sending_flits.push(flit);
    }

    return true;
}

bool Encoding_RAW::decode(vector < Flit > &received_flits, Packet &packet) {
    double flit_keep_rate = 1.0 - GlobalParams::wired_flit_loss_rate;
    int wired_fl = 0, wired_be = 0, wireless_fl = 0, wireless_be = 0;

    for (auto &&flit : received_flits)
    {
        // calculate flit loss rate
        if (pow(flit_keep_rate, flit.meta.hop_no) < rand01())
        {
            // if at least one flit loss, it cant be decode.
            onDecodeFailure();
            return false;
        }

        // counting flipping-bit chance
        wired_fl += flit.meta.hop_no;
        wireless_fl += flit.meta.hub_hop_no;
        wired_be += flit.meta.hop_no * flit.payload.data.length();
        wireless_be += flit.meta.hub_hop_no * flit.payload.data.length();
    }

    if (wired_fl > 0 && pseudo_prob_repeat(GlobalParams::wired_flit_loss_rate, wired_fl) > rand01())
    {
        onDecodeFailure();
        return false;
    }

    if (wireless_fl > 0 && pseudo_prob_repeat(GlobalParams::wireless_flit_loss_rate, wireless_fl) > rand01())
    {
        onDecodeFailure();
        return false;
    }
    
    if (wired_be > 0 && pseudo_prob_repeat(GlobalParams::wired_bit_error_rate, wired_be) > rand01())
    {
        onDecodeSuccess(false);
        return true;
    }
    
    if (wireless_be > 0 && pseudo_prob_repeat(GlobalParams::wireless_bit_error_rate, wireless_be) > rand01())
    {
        onDecodeSuccess(false);
        return true;
    }

    onDecodeSuccess(true);
    return true;
}
