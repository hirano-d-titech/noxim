#include "Encoding_HAMMING.h"

EncodingModelsRegister Encoding_HAMMING::encodingModelsRegister("HAMMING", getInstance());

Encoding_HAMMING * Encoding_HAMMING::encoding_HAMMING = 0;

Encoding_HAMMING * Encoding_HAMMING::getInstance() {
    if (encoding_HAMMING == 0)
        encoding_HAMMING = new Encoding_HAMMING();
    return encoding_HAMMING;
}

bool Encoding_HAMMING::encode(Packet &packet, queue < Flit > &sending_flits)
{
    auto size = (packet.size + 3) / 4 * 7; // multiple of 4
    packet.size = size;
    for (size_t i = 0; i < size; i++)
    {
        Flit flit(packet);
        flit.sequence_no = i;
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

bool Encoding_HAMMING::decode(vector < Flit > &received_flits, Packet &packet) {
    packet = reconstructPacket(received_flits);
    if (packet.size != (int)received_flits.size())
    {
        onDecodeFailure();
        return false;
    }

    if (packet.size % 7 != 0)
    {
        onDecodeFailure();
        return false;
    }

    int sum_hop_no = 0, sum_hub_hop_no = 0;
    // if flit lossed, it cant be decoded.
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

        sum_hop_no += flit.hop_no;
        sum_hub_hop_no += flit.hub_hop_no;
    }

    // HAMMING has proof to 1 error bit in 7 bits
    int avg_hop_no = (sum_hop_no + packet.size - 1) / packet.size;
    int avg_hub_hop_no = (sum_hub_hop_no + packet.size - 1) / packet.size;
    size_t loop = packet.size * received_flits[0].payload.data.length() / 7;
    for (size_t i = 0; i < loop; i++)
    {
        if (pesudo_prob_poisson(avg_hop_no, 2, GlobalParams::wired_bit_error_rate) > rand01())
        {
            onDecodeSuccess(false);
            return true;
        }

        if (pesudo_prob_poisson(avg_hub_hop_no, 2, GlobalParams::wireless_bit_error_rate) > rand01())
        {
            onDecodeSuccess(false);
            return true;
        }
    }

    onDecodeSuccess(true);
    return true;
}