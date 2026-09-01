/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the implementation of the processing element
 */

#include "ProcessingElement.h"



int ProcessingElement::randInt(int min, int max)
{
  return min + (int) ((double) (max - min + 1) * rand() / (RAND_MAX + 1.0));
}

void ProcessingElement::rxProcess()
{
  if (reset.read())
  {
    ack_rx.write(0);
    current_level_rx = 0;
    ack_req.write(Ack());
  }
  else
  {
    bool tail_received = false;
    bool decode_success = true;
    Packet received_packet;

    if (req_rx.read() == 1 - current_level_rx)
    {
      Flit flit_next = flit_rx.read();
      flit_buffer.push_back(flit_next);
      if (flit_next.flit_type == FLIT_TYPE_TAIL) {
        decode_success = encodingModel->decode(flit_buffer, received_packet);
        
        bool virtual_decode_success = true;
        if (!flit_buffer.empty()) {
          const ClusterEncodingMeta &meta = flit_buffer[0].cluster_enc_meta;
          const std::map<int, int> &errors_map = flit_buffer[0].virtual_errors;
          
          // LIFO scan (from last passed cluster to first)
          for (int idx = meta.encoding_history_index - 1; idx >= 0; idx--) {
            int cluster_id = meta.cluster_history[idx];
            ClusterEncodingType enc_type = meta.encoding_history[idx];
            
            int errors = 0;
            auto err_it = errors_map.find(cluster_id);
            if (err_it != errors_map.end()) {
              errors = err_it->second;
            }
            
            // Learned locally by this PE (as receiver); no longer shipped back to the sender via Ack.
            if (enc_type == CLUSTER_ENC_PARITY) {
              if (errors == 0) {
                cluster_evaluations[cluster_id] += GlobalParams::eval_success;
              } else {
                cluster_evaluations[cluster_id] += GlobalParams::eval_fatal;
                virtual_decode_success = false;
                break; // Stop scanning inner clusters
              }
            } else if (enc_type == CLUSTER_ENC_SECDED) {
              if (errors == 0) {
                cluster_evaluations[cluster_id] += GlobalParams::eval_success;
              } else if (errors == 1) {
                cluster_evaluations[cluster_id] += GlobalParams::eval_corrected;
              } else {
                cluster_evaluations[cluster_id] += GlobalParams::eval_fatal;
                virtual_decode_success = false;
                break; // Stop scanning inner clusters
              }
            }
          }
        }
        
        decode_success = decode_success && virtual_decode_success;
        tail_received = true;
        flit_buffer.clear();
      }
      current_level_rx = 1 - current_level_rx;  // Negate the old value for Alternating Bit Protocol (ABP)
    }
    ack_rx.write(current_level_rx);

    // send ack on end of packet reception
    if (tail_received) {
      if (GlobalParams::verbose_mode != VERBOSE_OFF) {
        cout << "PE " << local_id << " @ " << sc_time_stamp().to_double() / GlobalParams::clock_period_ps 
             << ": Received tail flit of packet_id " << received_packet.packet_id 
             << " from PE " << received_packet.src_id
             << ", decode " << (decode_success ? "SUCCESS (sending ACK)" : "FAILURE (sending NACK)") << endl;
      }
      Ack ack_signal(received_packet.src_id, received_packet.dst_id, received_packet.packet_id, !decode_success);
      ack_req.write(ack_signal);
    } else {
      // fill invalid ack to avoid uninitialized signal issues
      ack_req.write(Ack());
    }
  }
}

void ProcessingElement::txProcess()
{
  if (reset.read())
  {
    req_tx.write(0);
    current_level_tx = 0;
    transmittedAtPreviousCycle = false;
    outstanding_packets.clear();
    last_recovery_cycle = 0;
  }
  else
  {
    double current_cycle = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
    int current_cycle_int = static_cast<int>(current_cycle);

    // Recovery of negative cluster evaluations over time
    if (current_cycle_int > 0 &&
        current_cycle_int % GlobalParams::recovery_interval == 0 &&
        current_cycle_int != last_recovery_cycle) {
      last_recovery_cycle = current_cycle_int;
      for (auto &eval : cluster_evaluations) {
        if (eval.second < 0.0) {
          eval.second += 1.0;
          if (eval.second > 0.0) {
            eval.second = 0.0;
          }
          if (GlobalParams::verbose_mode != VERBOSE_OFF) {
            cout << "PE " << local_id << " @ " << current_cycle << ": Recovered cluster " << eval.first << " evaluation to " << eval.second << endl;
          }
        }
      }
      routing_manager.recalculateAllRoutes(local_id, cluster_evaluations);
    }

    // check for incoming ACKs/NACKs
    const Ack &incoming_ack = ack_ack.read();
    if (incoming_ack.isValid() && incoming_ack.src_id == local_id) {
      auto it = outstanding_packets.find(incoming_ack.packet_id);
      if (it != outstanding_packets.end()) {
        // Ack carries success/failure only (no per-cluster detail): apply a uniform
        // reward/penalty to every cluster on the route this packet took. Repeated
        // outcomes on the same cluster accumulate, so a cluster common to many
        // failing routes (e.g. near the destination) is penalized the most.
        const vector<int> &route = it->second.packet.route_metadata.custom_data;
        double delta = incoming_ack.is_nack ? GlobalParams::eval_fatal : GlobalParams::eval_success;
        if (route.size() >= 2) {
          for (size_t i = 1; i < route.size(); i++) {
            cluster_evaluations[route[i]] += delta;
          }
        }

        if (incoming_ack.is_nack) {
          // NACK: Retransmit immediately
          if (GlobalParams::verbose_mode != VERBOSE_OFF) {
            cout << "PE " << local_id << " @ " << current_cycle << ": NACK received for packet_id " << incoming_ack.packet_id << ", triggering immediate retransmission (retransmit_count: " << it->second.retransmit_count + 1 << ")" << endl;
          }
          retransmitPacket(incoming_ack.packet_id);
          GlobalParams::total_retransmissions++;
          it->second.sent_time = current_cycle;
          it->second.retransmit_count++;
        } else {
          // ACK: Success, erase from map
          if (GlobalParams::verbose_mode != VERBOSE_OFF) {
            cout << "PE " << local_id << " @ " << current_cycle << ": ACK received for packet_id " << incoming_ack.packet_id << ", successfully delivered after " << it->second.retransmit_count << " retransmissions." << endl;
          }
          outstanding_packets.erase(it);
        }
      }
    }

    // timeout polling
    for (auto &pair : outstanding_packets) {
      SentPacketInfo &info = pair.second;
      Coord src = id2Coord(info.packet.src_id);
      Coord dst = id2Coord(info.packet.dst_id);
      int manhattan_dist = abs(src.x - dst.x) + abs(src.y - dst.y);
      double timeout_limit = GlobalParams::timeout_base_cycles + GlobalParams::timeout_factor_cycles * manhattan_dist;

      if (current_cycle - info.sent_time > timeout_limit) {
        // Timeout! Retransmit
        if (GlobalParams::verbose_mode != VERBOSE_OFF) {
          cout << "PE " << local_id << " @ " << current_cycle << ": Timeout (" << (current_cycle - info.sent_time) << " > " << timeout_limit << ") for packet_id " << info.packet.packet_id << ", retransmitting (retransmit_count: " << info.retransmit_count + 1 << ")" << endl;
        }
        retransmitPacket(info.packet.packet_id);
        GlobalParams::total_retransmissions++;
        info.sent_time = current_cycle;
        info.retransmit_count++;
      }
    }

    Packet packet;

    if (canShot(packet))
    {
      if (GlobalParams::routing_algorithm == "CLUSTER") {
        packet.route_metadata.custom_data = routing_manager.getRoute(packet.src_id, packet.dst_id);
      }
      packet.packet_id = packet_seq_num++;
      packet_queue.push(packet);
      transmittedAtPreviousCycle = true;
    }
    else
    {
      transmittedAtPreviousCycle = false;
    }

    if (ack_tx.read() == current_level_tx)
    {
      if (!packet_queue.empty())
      {
        Packet p_info = packet_queue.front();
        Flit flit = nextFlit();  // Generate a new flit
        flit_tx->write(flit);  // Send the generated flit
        current_level_tx = 1 - current_level_tx;  // Negate the old value for Alternating Bit Protocol (ABP)
        req_tx.write(current_level_tx);

        if (flit.flit_type == FLIT_TYPE_TAIL) {
          auto it = outstanding_packets.find(p_info.packet_id);
          if (it != outstanding_packets.end()) {
            it->second.sent_time = current_cycle;
            if (GlobalParams::verbose_mode != VERBOSE_OFF) {
              cout << "PE " << local_id << " @ " << current_cycle << ": Retransmitted tail flit for packet_id " << p_info.packet_id << " sent" << endl;
            }
          } else {
            SentPacketInfo info;
            info.packet = p_info;
            info.sent_time = current_cycle;
            info.retransmit_count = 0;
            outstanding_packets[p_info.packet_id] = info;
            if (GlobalParams::verbose_mode != VERBOSE_OFF) {
              cout << "PE " << local_id << " @ " << current_cycle << ": New packet_id " << p_info.packet_id << " tail flit sent, registered outstanding" << endl;
            }
          }
        }
      }
    }
  }
}

Flit ProcessingElement::nextFlit()
{
  if (front_packet_flits.empty())
  {
    encodingModel->encode(packet_queue.front(), front_packet_flits);
  }

  Flit flit = front_packet_flits.front();
  front_packet_flits.pop();

  if (front_packet_flits.empty())
    packet_queue.pop();

  return flit;
}

bool ProcessingElement::canShot(Packet & packet)
{
  // assert(false);
  if(never_transmit) return false;

  // if(local_id!=16) return false;

  /* DEADLOCK TEST
  double current_time = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;

  if (current_time >= 4100)
  {
    if (current_time==3500)
      cout << name() << " IN CODA " << packet_queue.size() << endl;
    return false;
  }
  */

#ifdef DEADLOCK_AVOIDANCE
  if (local_id%2==0) return false;
#endif
  bool shot;
  double threshold;

  double now = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;

  if (GlobalParams::traffic_distribution != TRAFFIC_TABLE_BASED)
  {
    threshold = GlobalParams::packet_injection_rate;

    shot = (((double) rand()) / RAND_MAX < threshold);
    if (shot) {
      if (GlobalParams::traffic_distribution == TRAFFIC_RANDOM)
      packet = trafficRandom();
      else if (GlobalParams::traffic_distribution == TRAFFIC_TRANSPOSE1)
      packet = trafficTranspose1();
      else if (GlobalParams::traffic_distribution == TRAFFIC_TRANSPOSE2)
      packet = trafficTranspose2();
      else if (GlobalParams::traffic_distribution == TRAFFIC_BIT_REVERSAL)
      packet = trafficBitReversal();
      else if (GlobalParams::traffic_distribution == TRAFFIC_SHUFFLE)
      packet = trafficShuffle();
      else if (GlobalParams::traffic_distribution == TRAFFIC_LOCAL)
      packet = trafficLocal();
      else if (GlobalParams::traffic_distribution == TRAFFIC_ULOCAL)
      packet = trafficULocal();
      else {
          cout << "Invalid traffic distribution: " << GlobalParams::traffic_distribution << endl;
          exit(-1);
      }
    }
  }
  else // Table based communication traffic
  {
    if (never_transmit) return false;

    bool use_pir = (transmittedAtPreviousCycle == false);
    vector < pair < int, double > > dst_prob;
    double threshold =
      traffic_table->getCumulativePirPor(local_id, (int) now, use_pir, dst_prob);

    double prob = (double) rand() / RAND_MAX;
    shot = (prob < threshold);
    if (shot)
    {
      for (unsigned int i = 0; i < dst_prob.size(); i++)
      {
        if (prob < dst_prob[i].second) {
            int vc = randInt(0,GlobalParams::n_virtual_channels-1);
            packet.make(local_id, dst_prob[i].first, vc, now, getRandomSize());
            break;
        }
      }
    }
  }

  return shot;
}

Packet ProcessingElement::trafficLocal()
{
  Packet p;
  p.src_id = local_id;
  double rnd = rand() / (double) RAND_MAX;

  vector<int> dst_set;

  int max_id = (GlobalParams::mesh_dim_x * GlobalParams::mesh_dim_y);

  for (int i=0;i<max_id;i++)
  {
    if (rnd<=GlobalParams::locality)
    {
        if (local_id!=i) dst_set.push_back(i);
    }
    else
      dst_set.push_back(i);
  }

  int i_rnd = rand()%dst_set.size();

  p.dst_id = dst_set[i_rnd];
  p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
  p.size = p.flit_left = getRandomSize();
  p.vc_id = randInt(0,GlobalParams::n_virtual_channels-1);

  return p;
}


int ProcessingElement::findRandomDestination(int id, int hops)
{
  assert(GlobalParams::topology == TOPOLOGY_MESH);

  int inc_y = rand()%2?-1:1;
  int inc_x = rand()%2?-1:1;

  Coord current =  id2Coord(id);

  for (int h = 0; h<hops; h++)
  {
    if (current.x==0)
        if (inc_x<0) inc_x=0;

    if (current.x== GlobalParams::mesh_dim_x-1)
        if (inc_x>0) inc_x=0;

    if (current.y==0)
        if (inc_y<0) inc_y=0;

    if (current.y==GlobalParams::mesh_dim_y-1)
        if (inc_y>0) inc_y=0;

    if (rand()%2)
        current.x +=inc_x;
    else
        current.y +=inc_y;
  }
  return coord2Id(current);
}


int roulette()
{
  int slices = GlobalParams::mesh_dim_x + GlobalParams::mesh_dim_y -2;
  double r = rand()/(double)RAND_MAX;

  for (int i=1;i<=slices;i++)
  {
    if (r< (1-1/double(2<<i)))
    {
        return i;
    }
  }
  assert(false);
  return 1;
}


Packet ProcessingElement::trafficULocal()
{
  Packet p;
  p.src_id = local_id;

  int target_hops = roulette();

  p.dst_id = findRandomDestination(local_id,target_hops);

  p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
  p.size = p.flit_left = getRandomSize();
  p.vc_id = randInt(0,GlobalParams::n_virtual_channels-1);

  return p;
}

Packet ProcessingElement::trafficRandom()
{
  Packet p;
  p.src_id = local_id;
  double rnd = rand() / (double) RAND_MAX;
  double range_start = 0.0;

  int max_id;

  if (GlobalParams::topology == TOPOLOGY_MESH)
    max_id = (GlobalParams::mesh_dim_x * GlobalParams::mesh_dim_y) - 1; //Mesh 
  else    // other topologies does not exist
    assert(false);

  // Random destination distribution
  do {
    p.dst_id = randInt(0, max_id);

  // check for hotspot destination
    for (size_t i = 0; i < GlobalParams::hotspots.size(); i++)
    {
      if (rnd >= range_start && rnd < range_start + GlobalParams::hotspots[i].second) {
        if (local_id != GlobalParams::hotspots[i].first ) {
            p.dst_id = GlobalParams::hotspots[i].first;
        }
        break;
      } else
      range_start += GlobalParams::hotspots[i].second;  // try next
    }
#ifdef DEADLOCK_AVOIDANCE
    assert((GlobalParams::topology == TOPOLOGY_MESH));
    if (p.dst_id%2!=0)
    {
        p.dst_id = (p.dst_id+1)%256;
    }
#endif
  } while (p.dst_id == p.src_id);

  p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
  p.size = p.flit_left = getRandomSize();
  p.vc_id = randInt(0,GlobalParams::n_virtual_channels-1);

  return p;
}
// TODO: for testing only
Packet ProcessingElement::trafficTest()
{
  Packet p;
  p.src_id = local_id;
  p.dst_id = 10;

  p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
  p.size = p.flit_left = getRandomSize();
  p.vc_id = randInt(0,GlobalParams::n_virtual_channels-1);

  return p;
}

Packet ProcessingElement::trafficTranspose1()
{
  assert(GlobalParams::topology == TOPOLOGY_MESH);
  Packet p;
  p.src_id = local_id;
  Coord src, dst;

  // Transpose 1 destination distribution
  src.x = id2Coord(p.src_id).x;
  src.y = id2Coord(p.src_id).y;
  dst.x = GlobalParams::mesh_dim_x - 1 - src.y;
  dst.y = GlobalParams::mesh_dim_y - 1 - src.x;
  fixRanges(src, dst);
  p.dst_id = coord2Id(dst);

  p.vc_id = randInt(0,GlobalParams::n_virtual_channels-1);
  p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
  p.size = p.flit_left = getRandomSize();

  return p;
}

Packet ProcessingElement::trafficTranspose2()
{
  assert(GlobalParams::topology == TOPOLOGY_MESH);
  Packet p;
  p.src_id = local_id;
  Coord src, dst;

  // Transpose 2 destination distribution
  src.x = id2Coord(p.src_id).x;
  src.y = id2Coord(p.src_id).y;
  dst.x = src.y;
  dst.y = src.x;
  fixRanges(src, dst);
  p.dst_id = coord2Id(dst);

  p.vc_id = randInt(0,GlobalParams::n_virtual_channels-1);
  p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
  p.size = p.flit_left = getRandomSize();

  return p;
}

void ProcessingElement::setBit(int &x, int w, int v)
{
  int mask = 1 << w;

  if (v == 1)
    x = x | mask;
  else if (v == 0)
    x = x & ~mask;
  else
    assert(false);
}

int ProcessingElement::getBit(int x, int w)
{
  return (x >> w) & 1;
}

inline double ProcessingElement::log2ceil(double x)
{
  return ceil(log(x) / log(2.0));
}

Packet ProcessingElement::trafficBitReversal()
{

  int nbits = (int)log2ceil((double)(GlobalParams::mesh_dim_x * GlobalParams::mesh_dim_y));
  int dnode = 0;
  for (int i = 0; i < nbits; i++)
    setBit(dnode, i, getBit(local_id, nbits - i - 1));

  Packet p;
  p.src_id = local_id;
  p.dst_id = dnode;

  p.vc_id = randInt(0,GlobalParams::n_virtual_channels-1);
  p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
  p.size = p.flit_left = getRandomSize();

  return p;
}

Packet ProcessingElement::trafficShuffle()
{
  int nbits = (int)log2ceil((double)(GlobalParams::mesh_dim_x * GlobalParams::mesh_dim_y));
  int dnode = 0;
  for (int i = 0; i < nbits - 1; i++)
    setBit(dnode, i + 1, getBit(local_id, i));
  setBit(dnode, 0, getBit(local_id, nbits - 1));

  Packet p;
  p.src_id = local_id;
  p.dst_id = dnode;

  p.vc_id = randInt(0,GlobalParams::n_virtual_channels-1);
  p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
  p.size = p.flit_left = getRandomSize();

  return p;
}

void ProcessingElement::fixRanges(const Coord src,
               Coord & dst)
{
  // Fix ranges
  if (dst.x < 0)
    dst.x = 0;
  if (dst.y < 0)
    dst.y = 0;
  if (dst.x >= GlobalParams::mesh_dim_x)
    dst.x = GlobalParams::mesh_dim_x - 1;
  if (dst.y >= GlobalParams::mesh_dim_y)
    dst.y = GlobalParams::mesh_dim_y - 1;
}

int ProcessingElement::getRandomSize()
{
  return randInt(GlobalParams::min_packet_size,
    GlobalParams::max_packet_size);
}

unsigned int ProcessingElement::getQueueSize() const
{
  return packet_queue.size();
}

void ProcessingElement::retransmitPacket(int packet_id)
{
  auto it = outstanding_packets.find(packet_id);
  if (it != outstanding_packets.end())
  {
    Packet p = it->second.packet;
    p.flit_left = p.size;
    packet_queue.push(p);
  }
}

