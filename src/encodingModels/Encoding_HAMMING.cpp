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
  onDecodeSuccess(true);
  return true;
}
