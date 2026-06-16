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
    flit.sequence_no = i;
    flit.payload = payloads[i % packet.flit_left];

    if (i == 0)
      flit.flit_type = FLIT_TYPE_HEAD;
    else if (i == repeated-1)
      flit.flit_type = FLIT_TYPE_TAIL;
    else
      flit.flit_type = FLIT_TYPE_BODY;

    sending_flits.push(flit);
  }

  return true;
}

bool Encoding_REPEAT::decode(vector < Flit > &received_flits, Packet &packet) {
  packet = reconstructPacket(received_flits);
  onDecodeSuccess(true);
  return true;
}
