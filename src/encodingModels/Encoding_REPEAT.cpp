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
        Flit flit(packet);
        flit.meta.sequence_no = i;
        flit.payload = payloads[i % packet.flit_left];
        flit.meta.virtual_encoding = false;

        flit.meta.hub_relay_node = NOT_VALID;

        if (i == 0)
            flit.meta.flit_type = FLIT_TYPE_HEAD;
        else if (i == repeated-1)
            flit.meta.flit_type = FLIT_TYPE_TAIL;
        else
            flit.meta.flit_type = FLIT_TYPE_BODY;

        sending_flits.push(flit);
    }

    return true;
}

bool Encoding_REPEAT::decode(vector < Flit > &received_flits, Packet &packet) {
    vector < Payload > received, predicted;
    if (!predictPayloadsOver(received_flits, received, predicted))
    {
        onDecodeFailure();
        return false;
    }

    size_t length = received.size();
    if (length % REPETITION != 0)
    {
        onDecodeFailure();
        return false;
    }

    size_t size = length / REPETITION;
    vector < Payload > decoded;
    decoded.reserve(size);

    for (size_t i = 0; i < size; i++)
    {
        Payload corrected = {0};

        for (int bit = 0; bit < corrected.data.length(); bit++)
        {
            int ones = 0;

            for (size_t j = 0; j < REPETITION; j++)
            {
                if (received[i + j * REPETITION].data[bit].to_bool()) {
                    ones++;
                }
            }

            corrected.data[bit] = ones > (REPETITION / 2) ? 1 : 0;
        }

        decoded[i] = corrected;
    }

    onDecodeSuccess(verifyPayloads(decoded, predicted));
    return true;
}
