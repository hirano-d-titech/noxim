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
    auto size = (packet.size + 3) / 4 * 4; // multiple of 4
    auto payloads = generatePayloads(packet);

    size_t total_codewords = size * 32 / 4;
    size_t total_output_bits = total_codewords * 7;
    int output_size = total_output_bits / 32;
    packet.size = output_size;
    vector<sc_uint<32>> encoded_data(output_size, 0);

    size_t input_bit_index = 0;
    size_t output_bit_index = 0;
    for (size_t cw = 0; cw < total_codewords; ++cw) {
        uint8_t D1 = (payloads[input_bit_index / 32].data.to_uint() >> (input_bit_index % 32)) & 0x1;
        uint8_t D2 = (payloads[(input_bit_index + 1) / 32].data.to_uint() >> ((input_bit_index + 1) % 32)) & 0x1;
        uint8_t D3 = (payloads[(input_bit_index + 2) / 32].data.to_uint() >> ((input_bit_index + 2) % 32)) & 0x1;
        uint8_t D4 = (payloads[(input_bit_index + 3) / 32].data.to_uint() >> ((input_bit_index + 3) % 32)) & 0x1;

        input_bit_index += 4;

        uint8_t P1 = D1 ^ D2 ^ D4;
        uint8_t P2 = D1 ^ D3 ^ D4;
        uint8_t P3 = D2 ^ D3 ^ D4;

        uint8_t hamming_code = (P1 << 6) | (P2 << 5) | (D1 << 4) | (P3 << 3) | (D2 << 2) | (D3 << 1) | D4;

        for (int bit = 6; bit >= 0; --bit) { // from MSB to LSB
            uint32_t bit_val = (hamming_code >> bit) & 0x1;
            size_t out_word = output_bit_index / 32;
            size_t out_bit = output_bit_index % 32;
            encoded_data[out_word] |= (bit_val << out_bit);
            output_bit_index++;
        }
    }

    for (auto i = 0; i < output_size; i++)
    {
        auto type = i == 0 ? FLIT_TYPE_HEAD :
                    i == output_size-1 ? FLIT_TYPE_TAIL :
                    FLIT_TYPE_BODY;

        Flit flit(packet, type);
        flit.meta.sequence_no = i;
        flit.payload = Payload{encoded_data[i]};

        sending_flits.push(flit);
    }

    return true;
}

bool Encoding_HAMMING::decode(vector < Flit > &received_flits, Packet &packet) {
    vector < Payload > received, predicted, filled, decoded;
    simulate_hops(received_flits);

    if (!predictPayloadsOver(received_flits, received, predicted, packet))
    {
        onDecodeFailure();
        return false;
    }

    size_t length = received_flits[0].meta.sequence_length;
    assert(length % 7 == 0);

    for (size_t i = 0; i < received_flits.size(); i++)
    {
        while (received_flits[i].meta.sequence_no > (int)filled.size()) filled.push_back(Payload{0});
        filled.push_back(received[i]);
    }
    while (length > filled.size()) filled.push_back(Payload{0});

    size_t total_input_bits = length * 32;
    size_t total_codewords = total_input_bits / 7;
    size_t total_output_bits = total_codewords * 4;
    size_t output_size = total_output_bits / 32;

    for (size_t i = 0; i < output_size; i++)
    {
        decoded.push_back(Payload{0});
    }

    size_t input_bit_index = 0;
    size_t output_bit_index = 0;

    auto get_bit = [&](size_t bit_idx) -> uint8_t {
        size_t word = bit_idx / 32;
        return word >= length ? 0 : (filled[word].data.to_uint() >> bit_idx % 32) & 0x1;
    };

    auto set_bit = [&](size_t bit_idx, uint8_t value) {
        size_t word = bit_idx / 32;
        size_t bit = bit_idx % 32;
        if (word >= decoded.size()) return;
        if (value)
            decoded[word].data |= (1u << bit);
        else
            decoded[word].data &= ~(1u << bit);
    };

    for (size_t cw = 0; cw < total_codewords; ++cw) {
        uint8_t P1 = get_bit(input_bit_index++);
        uint8_t P2 = get_bit(input_bit_index++);
        uint8_t D1 = get_bit(input_bit_index++);
        uint8_t P3 = get_bit(input_bit_index++);
        uint8_t D2 = get_bit(input_bit_index++);
        uint8_t D3 = get_bit(input_bit_index++);
        uint8_t D4 = get_bit(input_bit_index++);

        uint8_t S1 = P1 ^ D1 ^ D2 ^ D4;
        uint8_t S2 = P2 ^ D1 ^ D3 ^ D4;
        uint8_t S3 = P3 ^ D2 ^ D3 ^ D4;

        uint8_t syndrome = (S3 << 2) | (S2 << 1) | S1;

        if (syndrome != 0) {
            if (syndrome >= 1 && syndrome <= 7) {
                size_t error_bit_in_codeword = syndrome - 1;
                size_t error_bit_global = cw * 7 + error_bit_in_codeword;
                if (error_bit_global >= total_input_bits) {
                    onDecodeFailure();
                    return false;
                }
                uint8_t current_bit = get_bit(error_bit_global);
                uint8_t corrected_bit = current_bit ^ 0x1;
                set_bit(error_bit_global, corrected_bit);
            } else {
                onDecodeFailure();
                return false;
            }
        }

        // データビットを出力に追加
        set_bit(output_bit_index++, D1);
        set_bit(output_bit_index++, D2);
        set_bit(output_bit_index++, D3);
        set_bit(output_bit_index++, D4);
    }

    onDecodeSuccess(verifyPayloads(decoded, predicted));
    return true;
}