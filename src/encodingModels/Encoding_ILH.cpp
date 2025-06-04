#include "Encoding_ILH.h"

EncodingModelsRegister Encoding_ILH::encodingModelsRegister("ILH", getInstance());

Encoding_ILH * Encoding_ILH::encoding_ILH = 0;

Encoding_ILH * Encoding_ILH::getInstance() {
    if (encoding_ILH == 0)
        encoding_ILH = new Encoding_ILH();
    return encoding_ILH;
}

bool Encoding_ILH::encode(Packet &packet, queue < Flit > &sending_flits)
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

bool Encoding_ILH::decode(vector < Flit > &received_flits, Packet &packet) {
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

    // simulate loss position
    bool il_code[32*packet.size] = {0};
    for (int i = 0; i < packet.size; i++)
    {
        auto flit = received_flits[0];
        for (int it = 0; it < flit.hop_no; it++)
        {
            if (GlobalParams::wired_flit_loss_rate > rand01())
            {
                for (int b = 0; b < 32; b++)
                {
                    il_code[i*32 + b] = 1;
                }
                break;
            }

            if (GlobalParams::wired_bit_error_rate > rand01())
            {
                il_code[i*32 + rand() % 32] = 1;
            }
        }

        for (int it = 0; it < flit.hub_hop_no; it++)
        {
            if (GlobalParams::wireless_flit_loss_rate > rand01())
            {
                for (int b = 0; b < 32; b++)
                {
                    il_code[i*32 + b] = 1;
                }
                break;
            }

            if (GlobalParams::wireless_bit_error_rate > rand01())
            {
                il_code[i*32 + rand() % 32] = 1;
            }
        }
    }

    int interval = 32 * packet.size / 7;
    for (int i = 0; i < interval; i++)
    {
        int miss = 0;
        for (int b = 0; b < 7; b++)
        {
            miss += il_code[b * interval + i];
        }
        if (miss > 1)
        {
            onDecodeFailure();
            return false;
        }
    }

    onDecodeSuccess(true);
    return true;
}
