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
  onDecodeSuccess(true);
  return true;
}
