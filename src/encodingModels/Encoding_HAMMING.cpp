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
    packet.size = size;
    auto payloads = generatePayloads(packet);

    size_t total_codewords = size * 32 / 4;
    size_t total_output_bits = total_codewords * 7;
    size_t output_size = total_output_bits / 32;
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
                    i == size-1 ? FLIT_TYPE_TAIL :
                    FLIT_TYPE_BODY;

        Flit flit(packet, type);
        flit.meta.sequence_no = i;
        flit.payload = Payload{encoded_data[i]};

        sending_flits.push(flit);
    }

    return true;
}

bool Encoding_HAMMING::decode(vector < Flit > &received_flits, Packet &packet) {
 
}

sc_uint<7> Encoding_HAMMING::hamming_encode(sc_uint<4> data)
{
    sc_uint<7> encoded;

    encoded[0] = data[0] ^ data[1] ^ data[3]; // p1 = d1 ⊕ d2 ⊕ d4
    encoded[1] = data[0] ^ data[2] ^ data[3]; // p2 = d1 ⊕ d3 ⊕ d4
    encoded[2] = data[0]; // d1
    encoded[3] = data[1] ^ data[2] ^ data[3]; // p3 = d2 ⊕ d3 ⊕ d4
    encoded[4] = data[1]; // d2
    encoded[5] = data[2]; // d3
    encoded[6] = data[3]; // d4

    return encoded;
}

sc_uint<4> Encoding_HAMMING::hamming_decode(sc_uint<7> data)
{

}
