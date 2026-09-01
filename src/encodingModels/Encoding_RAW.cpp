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
  packet = reconstructPacket(received_flits);
  onDecodeSuccess(true);
  return true;
}
