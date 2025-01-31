#include "Encoding_REPEAT.h"

EncodingModelsRegister Encoding_REPEAT::encodingModelsRegister("REPEAT", getInstance());

Encoding_REPEAT * Encoding_REPEAT::encoding_REPEAT = 0;

Encoding_REPEAT * Encoding_REPEAT::getInstance() {
    if (encoding_REPEAT == 0)
        encoding_REPEAT = new Encoding_REPEAT();
    return encoding_REPEAT;
}

bool Encoding_REPEAT::encode(Packet &packet, queue < Flit > &sending_flits) {
    int repeated = packet.flit_left*REPETITION;
    auto payloads = generatePayloads(packet);

    for (int i = 0; i < repeated; i++)
    {
        auto type = i == 0 ? FLIT_TYPE_HEAD :
                    i == packet.size-1 ? FLIT_TYPE_TAIL :
                    FLIT_TYPE_BODY;

        Flit flit(packet, type);
        flit.meta.sequence_length = repeated;
        flit.meta.sequence_no = i;
        flit.payload = payloads[i % packet.flit_left];

        sending_flits.push(flit);
    }

    return true;
}

bool Encoding_REPEAT::decode(vector < Flit > &received_flits, Packet &packet) {
    vector < Payload > received, predicted, decoded;
    simulate_hops(received_flits);

    if (!predictPayloadsOver(received_flits, received, predicted, packet))
    {
        onDecodeFailure();
        return false;
    }

    size_t length = received_flits[0].meta.sequence_length;

    map < int , Payload > filled;
    for (size_t i = 0; i < received_flits.size(); i++)
    {
        filled.emplace(received_flits[i].meta.sequence_no, received[i]);
    }

    assert(length % REPETITION == 0);
    size_t size = length / REPETITION;
    decoded.reserve(size);

    for (size_t i = 0; i < size; i++)
    {
        Payload corrected = {0};

        for (int bit = 0; bit < corrected.data.length(); bit++)
        {
            int all = 0, ones = 0;

            for (size_t j = 0; j < REPETITION; j++)
            {
                auto it = filled.find(i + j * REPETITION);
                if (it != filled.end())
                {
                    all++;
                    ones += it->second.data[bit].to_bool() ? 1 : 0;
                }
            }

            if (all == ones * 2)
            {
                onDecodeFailure();
                return false;
            }

            corrected.data[bit] = ones > (all / 2) ? 1 : 0;
        }

        decoded[i] = corrected;
    }

    onDecodeSuccess(verifyPayloads(decoded, predicted));
    return true;
}
