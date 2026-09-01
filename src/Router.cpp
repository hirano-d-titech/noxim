/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the implementation of the router
 */

#include "Router.h"
#include "NoC.h"

extern NoC *n;


inline int toggleKthBit(int n, int k)
{
  return (n ^ (1 << (k-1)));
}

void Router::process()
{
  if (reset.read())
  {
    txProcess();
    rxProcess();
  }
  else
  {
    active_in_current_cycle = false;
    txProcess();
    rxProcess();
    degradation_monitor.updateState(active_in_current_cycle);
  }
}

void Router::rxProcess()
{
  if (reset.read())
  {
    TBufferFullStatus bfs;
    // Clear outputs and indexes of receiving protocol
    for (int i = 0; i < DIRECTIONS + 1; i++) {
      ack_rx[i].write(0);
      current_level_rx[i] = 0;
      buffer_full_status_rx[i].write(bfs);
      for (int vc = 0; vc < MAX_VIRTUAL_CHANNELS; vc++) {
        while(!delay_buffer[i][vc].empty()) delay_buffer[i][vc].pop();
      }
    }
    routed_flits = 0;
    local_drained = 0;
  }
  else
  {
    // This process simply sees a flow of incoming flits. All arbitration
    // and wormhole related issues are addressed in the txProcess()
    //assert(false);
    for (int i = 0; i < DIRECTIONS + 1; i++)
    {
      // To accept a new flit, the following conditions must match:
      // 1) there is an incoming request
      // 2) there is a free slot in the input buffer of direction i
      //LOG<<"****RX****DIRECTION ="<<i<<  endl;

      if (req_rx[i].read() == 1 - current_level_rx[i])
      {
        Flit received_flit = flit_rx[i].read();
        received_flit.hop_no++;

        active_in_current_cycle = true;

        double loss_rate = degradation_monitor.getCurrentLossRate();
        if (loss_rate > 0.0 && (rand() / (RAND_MAX + 1.0)) < loss_rate) {
          LOG << " Flit " << received_flit << " dropped at Router " << local_id << " due to degradation wear (loss rate: " << loss_rate << ")" << endl;
        } else {
          double ber = degradation_monitor.getCurrentBER();
          if (ber > 0.0) {
            int flipped_bits = 0;
            for (int b = 0; b < GlobalParams::flit_size; b++) {
              if ((rand() / (RAND_MAX + 1.0)) < ber) {
                flipped_bits++;
              }
            }
            if (flipped_bits > 0) {
              received_flit.virtual_errors[my_cluster_id] += flipped_bits;
            }
          }

          double current_cycle = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
          int delay_val = degradation_monitor.getCurrentDelay();
          int target_cycle = (int)current_cycle + delay_val;
          int vc = received_flit.vc_id;

          if (vc >= 0 && vc < MAX_VIRTUAL_CHANNELS) {
            delay_buffer[i][vc].push(make_pair(received_flit, target_cycle));
          } else {
            assert(false);
          }
        }

        current_level_rx[i] = 1 - current_level_rx[i];
      }

      ack_rx[i].write(current_level_rx[i]);

      // 遅延消化処理
      double current_cycle = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
      for (int vc = 0; vc < GlobalParams::n_virtual_channels; vc++)
      {
        if (!delay_buffer[i][vc].empty())
        {
          pair<Flit, int>& head = delay_buffer[i][vc].front();
          
          // 放出目標サイクルに達しているかチェック
          if ((int)current_cycle >= head.second)
          {
            Flit& flit_to_process = head.first;

            if (!buffer[i][vc].IsFull()) 
            {
              // バッファに空きがあれば投入し、キューから削除
              buffer[i][vc].Push(flit_to_process);
              LOG << " Flit " << flit_to_process << " collected from Input[" << i << "][" << vc <<"]" << endl;
              delay_buffer[i][vc].pop();
            }
            else
            {
              // バッファフル：投入もポップもせず、次のサイクルで再試行
              LOG << " Flit " << flit_to_process << " buffer full Input[" << i << "][" << vc <<"]" << endl;
              assert(i == DIRECTION_LOCAL);
            }
          }
        }
      }

      // updates the mask of VCs to prevent incoming data on full buffers
      // 遅延キューに保留されている未処理フリットもバッファ占有数に加算する
      int pending_count[MAX_VIRTUAL_CHANNELS] = {0};
      for (int vc = 0; vc < GlobalParams::n_virtual_channels; vc++) {
        pending_count[vc] = delay_buffer[i][vc].size();
      }

      TBufferFullStatus bfs;
      for (int vc=0;vc<GlobalParams::n_virtual_channels;vc++) {
        bfs.mask[vc] = (buffer[i][vc].Size() + pending_count[vc] >= buffer[i][vc].GetMaxBufferSize());
      }
      buffer_full_status_rx[i].write(bfs);
    }
  }
}

void Router::txProcess()
{
  if (reset.read())
  {
    // Clear outputs and indexes of transmitting protocol
    for (int i = 0; i < DIRECTIONS + 1; i++) 
    {
      req_tx[i].write(0);
      current_level_tx[i] = 0;
    }
    // Reset cluster encoding contexts
    for (int i = 0; i < DIRECTIONS + 1; i++)
      for (int vc = 0; vc < GlobalParams::n_virtual_channels; vc++)
        cluster_enc_ctx[i][vc].reset();
  }
  else
  {
    int current_cycle = (int)(sc_time_stamp().to_double() / GlobalParams::clock_period_ps);
    // 1st phase: Reservation
    for (int j = 0; j < DIRECTIONS + 1; j++)
    {
      int i = (start_from_port + j) % (DIRECTIONS + 1);

      for (int k = 0;k < GlobalParams::n_virtual_channels; k++)
      {
        int vc = (start_from_vc[i]+k)%(GlobalParams::n_virtual_channels);
        
        // Uncomment to enable deadlock checking on buffers. 
        // Please also set the appropriate threshold.
        // buffer[i].deadlockCheck();

        if (!buffer[i][vc].IsEmpty()) 
        {
          Flit & flit = buffer[i][vc].FrontRef();

          if (flit.flit_type == FLIT_TYPE_HEAD) 
          {
            // prepare data for routing
            RouteData route_data;
            route_data.current_id = local_id;
            //LOG<< "current_id= "<< route_data.current_id <<" for sending " << flit << endl;
            route_data.src_id = flit.src_id;
            route_data.dst_id = flit.dst_id;
            route_data.dir_in = i;
            route_data.vc_id = flit.vc_id;
            route_data.flit = &flit;

            // TODO: see PER POSTERI (adaptive routing should not recompute route if already reserved)
            int o = route(route_data);

            TReservation r;
            r.input = i;
            r.vc = vc;

            LOG << " checking availability of Output[" << o << "] for Input[" << i << "][" << vc << "] flit " << flit << endl;

            int rt_status = reservation_table.checkReservation(r,o);

            if (rt_status == RT_AVAILABLE) 
            {
              LOG << " reserving direction " << o << " for flit " << flit << endl;
              reservation_table.reserve(r, o);
            }
            else if (rt_status == RT_ALREADY_SAME)
            {
              LOG << " RT_ALREADY_SAME reserved direction " << o << " for flit " << flit << endl;
            }
            else if (rt_status == RT_OUTVC_BUSY)
            {
              LOG << " RT_OUTVC_BUSY reservation direction " << o << " for flit " << flit << endl;
            }
            else if (rt_status == RT_ALREADY_OTHER_OUT)
            {
              LOG  << "RT_ALREADY_OTHER_OUT: another output previously reserved for the same flit " << endl;
            }
            else assert(false); // no meaningful status here
          }
        }
      }
      start_from_vc[i] = (start_from_vc[i]+1)%GlobalParams::n_virtual_channels;
    }

    start_from_port = (start_from_port + 1) % (DIRECTIONS + 1);

    // Pre-forwarding: Process cluster encoding extra flit / release states
    processClusterEncoding();

    // 2nd phase: Forwarding
    //if (local_id==6) LOG<<"*TX*****local_id="<<local_id<<"__ack_tx[0]= "<<ack_tx[0].read()<<endl;
    for (int i = 0; i < DIRECTIONS + 1; i++) 
    {
      vector<pair<int,int> > reservations = reservation_table.getReservations(i);

      if (reservations.size()!=0)
      {
        int rnd_idx = rand()%reservations.size();

        int o = reservations[rnd_idx].first;
        int vc = reservations[rnd_idx].second;
        // LOG<< "found reservation from input= " << i << "_to output= "<<o<<endl;
        // can happen
        if (!buffer[i][vc].IsEmpty())  
        {
          ClusterEncContext & ctx = cluster_enc_ctx[o][vc];

          if (isClusterBoundaryCrossing(o))
          {
            if (ctx.state == CENC_EXTRA_FLIT || ctx.state == CENC_RELEASE_WAIT)
            {
              continue; // Skip normal forwarding to let processClusterEncoding handle the extra flit
            }
          }

          Flit flit = buffer[i][vc].Front();

          //LOG<< "*****TX***Direction= "<<i<< "************"<<endl;
          //LOG<<"_cl_tx="<<current_level_tx[o]<<"req_tx="<<req_tx[o].read()<<" _ack= "<<ack_tx[o].read()<< endl;

          bool cycle_allowed = true;
          if (isClusterBoundaryCrossing(o))
          {
            if (current_cycle <= ctx.last_processed_cycle)
              cycle_allowed = false;
          }

          if ( (current_level_tx[o] == ack_tx[o].read()) &&
              (buffer_full_status_tx[o].read().mask[vc] == false) &&
              cycle_allowed )
          {
            if (isClusterBoundaryCrossing(o))
            {
              if (flit.flit_type == FLIT_TYPE_HEAD)
              {
                ClusterEncodingType decided_type;
                int decided_redundancy;
                decideClusterEncodingType(o, vc, decided_type, decided_redundancy, flit.cluster_enc_meta.effective_bits, flit.src_id);

                if (decided_type == CLUSTER_ENC_NONE)
                {
                  ctx.reset();
                }
                else
                {
                  ctx.state = CENC_PROCESSING;
                  ctx.encoding_type = decided_type;
                  ctx.redundancy_bits = decided_redundancy;
                  ctx.input_port = i;
                  ctx.output_port = o;
                  ctx.last_processed_cycle = current_cycle;
                  ctx.original_sequence_length = flit.sequence_length;

                  ctx.effective_bits_after = flit.cluster_enc_meta.effective_bits + ctx.redundancy_bits;
                  ctx.needs_extra_flit = (ctx.effective_bits_after > ctx.original_sequence_length * GlobalParams::flit_size);

                  if (ctx.needs_extra_flit)
                  {
                    flit.sequence_length = ctx.original_sequence_length + 1;
                  }
                  flit.cluster_enc_meta.effective_bits = ctx.effective_bits_after;
                   if (flit.cluster_enc_meta.encoding_history_index < MAX_CLUSTER_HOPS)
                   {
                     flit.cluster_enc_meta.encoding_history[flit.cluster_enc_meta.encoding_history_index] = ctx.encoding_type;
                     flit.cluster_enc_meta.cluster_history[flit.cluster_enc_meta.encoding_history_index] = my_cluster_id;
                     flit.cluster_enc_meta.encoding_history_index++;
                   }

                  ctx.updated_enc_meta = flit.cluster_enc_meta;

                  LOG << " [ClusterEnc] Input[" << i << "][" << vc << "] HEAD boundary crossing to Output[" << o << "], decided_type=" << ctx.encoding_type << ", original_len=" << ctx.original_sequence_length << ", new_len=" << flit.sequence_length << ", eff_bits=" << ctx.effective_bits_after << ", needs_extra=" << ctx.needs_extra_flit << endl;
                }
              }
              else if (ctx.state == CENC_PROCESSING)
              {
                if (flit.flit_type == FLIT_TYPE_BODY)
                {
                  flit.sequence_length = ctx.needs_extra_flit ? ctx.original_sequence_length + 1 : ctx.original_sequence_length;
                  flit.cluster_enc_meta = ctx.updated_enc_meta;
                  ctx.last_processed_cycle = current_cycle;

                  LOG << " [ClusterEnc] Input[" << i << "][" << vc << "] BODY tracking crossing boundary to Output[" << o << "]" << endl;
                }
                else if (flit.flit_type == FLIT_TYPE_TAIL)
                {
                  flit.sequence_length = ctx.needs_extra_flit ? ctx.original_sequence_length + 1 : ctx.original_sequence_length;
                  flit.cluster_enc_meta = ctx.updated_enc_meta;
                  ctx.last_processed_cycle = current_cycle;

                  flit.payload.data = 0; // virtual packing

                  if (!ctx.needs_extra_flit)
                  {
                    LOG << " [ClusterEnc] Input[" << i << "][" << vc << "] TAIL boundary crossing to Output[" << o << "] (fits, no extra flit)" << endl;
                    ctx.reset();
                  }
                  else
                  {
                    flit.flit_type = FLIT_TYPE_BODY;

                    Flit extra_flit = flit;
                    extra_flit.flit_type = FLIT_TYPE_TAIL;
                    extra_flit.sequence_no = ctx.original_sequence_length;
                    extra_flit.payload.data = 0; // 0-filled

                    ctx.extra_flit = extra_flit;
                    ctx.state = CENC_EXTRA_FLIT;

                    LOG << " [ClusterEnc] Input[" << i << "][" << vc << "] TAIL boundary crossing to Output[" << o << "] (overflows, splitting and locking port)" << endl;
                  }
                }
              }
            }

            //if (GlobalParams::verbose_mode > VERBOSE_OFF)
            LOG << "Input[" << i << "][" << vc << "] forwarded to Output[" << o << "], flit: " << flit << endl;

            flit.hop_no++;
            flit_tx[o].write(flit);
            current_level_tx[o] = 1 - current_level_tx[o];
            req_tx[o].write(current_level_tx[o]);
            buffer[i][vc].Pop();
            active_in_current_cycle = true;

            if (flit.flit_type == FLIT_TYPE_TAIL)
            {
              TReservation r;
              r.input = i;
              r.vc = vc;
              reservation_table.release(r,o);
            }

            /* Stats ------------------------------------------------- */

            if (o == DIRECTION_LOCAL) 
            {
              LOG << "Consumed flit " << flit << endl;
              stats.receivedFlit(sc_time_stamp().to_double() / GlobalParams::clock_period_ps, flit);
              if (GlobalParams:: max_volume_to_be_drained) 
              {
                if (drained_volume >= GlobalParams:: max_volume_to_be_drained)
                  sc_stop();
                else
                {
                  drained_volume++;
                  local_drained++;
                }
              }
            }
            else if (i != DIRECTION_LOCAL) // not generated locally
              routed_flits++;
            /* End Stats ------------------------------------------------- */
            //LOG<<"END_OK_cl_tx="<<current_level_tx[o]<<"_req_tx="<<req_tx[o].read()<<" _ack= "<<ack_tx[o].read()<< endl;
          }
          else
          {
            LOG << " Cannot forward Input[" << i << "][" << vc << "] to Output[" << o << "], flit: " << flit << endl;
            //LOG << " **DEBUG APB: current_level_tx: " << current_level_tx[o] << " ack_tx: " << ack_tx[o].read() << endl;
            LOG << " **DEBUG buffer_full_status_tx " << buffer_full_status_tx[o].read().mask[vc] << endl;

            //LOG<<"END_NO_cl_tx="<<current_level_tx[o]<<"_req_tx="<<req_tx[o].read()<<" _ack= "<<ack_tx[o].read()<< endl;
            /*
            if (flit.flit_type == FLIT_TYPE_HEAD)
              reservation_table.release(i,flit.vc_id,o);
            */
          }
        }
      } // if not reserved 
      // else LOG<<"we have no reservation for direction "<<i<< endl;
    } // for loop directions

    if ((int)(sc_time_stamp().to_double() / GlobalParams::clock_period_ps)%2==0)
    reservation_table.updateIndex();
  }
}

NoP_data Router::getCurrentNoPData()
{
  NoP_data NoP_data;

  for (int j = 0; j < DIRECTIONS; j++) {
    try {
      NoP_data.channel_status_neighbor[j].free_slots = free_slots_neighbor[j].read();
      NoP_data.channel_status_neighbor[j].available = (reservation_table.isNotReserved(j));
    }
    catch (int e)
    {
        if (e!=NOT_VALID) assert(false);
        // Nothing to do if an NOT_VALID direction is caught
    };
  }

  NoP_data.sender_id = local_id;

  return NoP_data;
}

// ============================================================
// Cluster Boundary Encoding - Helper
// ============================================================
bool Router::isClusterBoundaryCrossing(int output_port) const
{
  if (output_port == DIRECTION_LOCAL || output_port < 0 || output_port >= DIRECTIONS)
    return false;
  int neighbor_id = getNeighborId(local_id, output_port);
  if (neighbor_id == NOT_VALID) return false;
  return getClusterId(local_id) != getClusterId(neighbor_id);
}

void Router::decideClusterEncodingType(int output_port, int vc_id, ClusterEncodingType &type, int &redundancy_bits, int effective_bits, int src_id)
{
  int neighbor_id = getNeighborId(local_id, output_port);
  int target_cluster_id = getClusterId(neighbor_id);

  double trust_score = 0.0;
  Tile *src_tile = n->searchNode(src_id);
  if (src_tile && src_tile->pe) {
    auto it = src_tile->pe->cluster_evaluations.find(target_cluster_id);
    if (it != src_tile->pe->cluster_evaluations.end()) {
      trust_score = it->second;
    }
  }

  if (trust_score <= 0.0) {
    type = CLUSTER_ENC_SECDED;
    // Calculate SECDED parity bits (2^p >= E + p + 1)
    int p = 3;
    while ((1 << p) < (effective_bits + p + 1)) {
      p++;
    }
    redundancy_bits = p + 1; // SECDED is Hamming + 1 parity bit
  } else {
    type = CLUSTER_ENC_PARITY;
    redundancy_bits = 1; // 1-bit parity
  }
}

// ============================================================
// Cluster Boundary Encoding - State Machine Processing
// Called at the beginning of txProcess (before normal forwarding)
// to handle CENC_EXTRA_FLIT and CENC_RELEASE_WAIT states.
// ============================================================
void Router::processClusterEncoding()
{
  int current_cycle = (int)(sc_time_stamp().to_double() / GlobalParams::clock_period_ps);

  for (int o = 0; o < DIRECTIONS + 1; o++)
  {
    for (int vc = 0; vc < GlobalParams::n_virtual_channels; vc++)
    {
      ClusterEncContext & ctx = cluster_enc_ctx[o][vc];

      if (ctx.state == CENC_EXTRA_FLIT)
      {
        // Send the extra TAIL flit on the output port
        if (current_level_tx[o] == ack_tx[o].read() &&
            buffer_full_status_tx[o].read().mask[vc] == false)
        {
          LOG << " [ClusterEnc] Sending extra TAIL flit on Output[" << o << "] VC[" << vc << "]" << endl;

           ctx.extra_flit.hop_no++;
           flit_tx[o].write(ctx.extra_flit);
           current_level_tx[o] = 1 - current_level_tx[o];
           req_tx[o].write(current_level_tx[o]);
           active_in_current_cycle = true;

           // Transition to RELEASE_WAIT
           ctx.state = CENC_RELEASE_WAIT;
           ctx.last_processed_cycle = current_cycle;
        }
      }
      else if (ctx.state == CENC_RELEASE_WAIT)
      {
        // Ensure at least 1 cycle has passed since extra flit was sent
        if (current_cycle > ctx.last_processed_cycle)
        {
          LOG << " [ClusterEnc] Releasing port Output[" << o << "] VC[" << vc << "] after extra TAIL" << endl;

          // Now release the reservation that was held
          TReservation r;
          r.input = ctx.input_port;
          r.vc = vc;
          reservation_table.release(r, o);

          // Stats for non-local, non-PE-generated flits
          if (o != DIRECTION_LOCAL && ctx.input_port != DIRECTION_LOCAL)
            routed_flits++;

          // Full reset of context
          ctx.reset();
        }
      }
    }
  }
}

void Router::perCycleUpdate()
{
  if (reset.read()) {
    for (int i = 0; i < DIRECTIONS + 1; i++)
      free_slots[i].write(buffer[i][DEFAULT_VC].GetMaxBufferSize());
  } else {
    selectionStrategy->perCycleUpdate(this);
  }
}

vector < int > Router::routingFunction(const RouteData & route_data)
{
  // TODO: fix all the deprecated verbose mode logs
  if (GlobalParams::verbose_mode > VERBOSE_OFF)
    LOG << "Wired routing for dst = " << route_data.dst_id << endl;

  if (route_data.flit != nullptr) {
    return routingAlgorithm->route(this, *route_data.flit, route_data);
  } else {
    Flit dummy_flit;
    return routingAlgorithm->route(this, dummy_flit, route_data);
  }
}

int Router::route(const RouteData & route_data)
{

  if (route_data.dst_id == local_id)
    return DIRECTION_LOCAL;

  vector < int >candidate_channels = routingFunction(route_data);

  return selectionFunction(candidate_channels, route_data);
}

void Router::NoP_report() const
{
  NoP_data NoP_tmp;
  LOG << "NoP report: " << endl;

  for (int i = 0; i < DIRECTIONS; i++) {
    NoP_tmp = NoP_data_in[i].read();
    if (NoP_tmp.sender_id != NOT_VALID)
      cout << NoP_tmp;
  }
}

//---------------------------------------------------------------------------

int Router::NoPScore(const NoP_data & nop_data,
        const vector < int >&nop_channels) const
{
  int score = 0;

  for (unsigned int i = 0; i < nop_channels.size(); i++)
  {
    int available;

    if (nop_data.channel_status_neighbor[nop_channels[i]].available)
      available = 1;
    else
      available = 0;

    int free_slots = nop_data.channel_status_neighbor[nop_channels[i]].free_slots;
    score += available * free_slots;
  }

  return score;
}

int Router::selectionFunction(const vector < int >&directions,
           const RouteData & route_data)
{
  // not so elegant but fast escape ;)
  if (directions.size() == 1)
    return directions[0];

  return selectionStrategy->apply(this, directions, route_data);
}

void Router::configure(const int _id,
          const double _warm_up_time,
          const unsigned int _max_buffer_size,
          GlobalRoutingTable & grt)
{
  local_id = _id;
  
  // Calculate my_cluster_id (assuming 2x2 clusters)
  int cx = local_id % GlobalParams::mesh_dim_x / 2;
  int cy = local_id / GlobalParams::mesh_dim_x / 2;
  int mesh_cx = (GlobalParams::mesh_dim_x + 1) / 2;
  my_cluster_id = cy * mesh_cx + cx;

  LOG << " [Degradation] Router " << local_id << " configured in cluster " << my_cluster_id << endl;

  stats.configure(_id, _warm_up_time);

  active_in_current_cycle = false;

  start_from_port = DIRECTION_LOCAL;


  if (grt.isValid())
    routing_table.configure(grt, _id);

  reservation_table.setSize(DIRECTIONS+1);

  for (int i = 0; i < DIRECTIONS + 1; i++)
  {
    for (int vc = 0; vc < GlobalParams::n_virtual_channels; vc++)
    {
        buffer[i][vc].SetMaxBufferSize(_max_buffer_size);
        buffer[i][vc].setLabel(string(name())+"->buffer["+i_to_string(i)+"]");
    }
    start_from_vc[i] = 0;
  }


  // Initialize cluster encoding contexts
  for (int i = 0; i < DIRECTIONS + 1; i++)
    for (int vc = 0; vc < GlobalParams::n_virtual_channels; vc++)
      cluster_enc_ctx[i][vc].reset();

  if (GlobalParams::topology == TOPOLOGY_MESH)
  {
    int row = _id / GlobalParams::mesh_dim_x;
    int col = _id % GlobalParams::mesh_dim_x;

    for (int vc = 0; vc<GlobalParams::n_virtual_channels; vc++)
    {
      if (row == 0)
        buffer[DIRECTION_NORTH][vc].Disable();
      if (row == GlobalParams::mesh_dim_y-1)
        buffer[DIRECTION_SOUTH][vc].Disable();
      if (col == 0)
        buffer[DIRECTION_WEST][vc].Disable();
      if (col == GlobalParams::mesh_dim_x-1)
        buffer[DIRECTION_EAST][vc].Disable();
    }
  }
}

unsigned long Router::getRoutedFlits()
{
  return routed_flits;
}

int Router::reflexDirection(int direction) const
{
  if (direction == DIRECTION_NORTH)
    return DIRECTION_SOUTH;
  if (direction == DIRECTION_EAST)
    return DIRECTION_WEST;
  if (direction == DIRECTION_WEST)
    return DIRECTION_EAST;
  if (direction == DIRECTION_SOUTH)
    return DIRECTION_NORTH;

  // you shouldn't be here
  assert(false);
  return NOT_VALID;
}

int Router::getNeighborId(int _id, int direction) const
{
  assert(GlobalParams::topology == TOPOLOGY_MESH);

  Coord my_coord = id2Coord(_id); 

  switch (direction) {
  case DIRECTION_NORTH:
    if (my_coord.y == 0)
      return NOT_VALID;
    my_coord.y--;
    break;
  case DIRECTION_SOUTH:
    if (my_coord.y == GlobalParams::mesh_dim_y - 1)
      return NOT_VALID;
    my_coord.y++;
    break;
  case DIRECTION_EAST:
    if (my_coord.x == GlobalParams::mesh_dim_x - 1)
      return NOT_VALID;
    my_coord.x++;
    break;
  case DIRECTION_WEST:
    if (my_coord.x == 0)
      return NOT_VALID;
    my_coord.x--;
    break;
  default:
    LOG << "Direction not valid : " << direction;
    assert(false);
  }

  int neighbor_id = coord2Id(my_coord);
  return neighbor_id;
}

bool Router::inCongestion()
{
  for (int i = 0; i < DIRECTIONS; i++)
  {
    if (free_slots_neighbor[i]==NOT_VALID) continue;

    int flits = GlobalParams::buffer_depth - free_slots_neighbor[i];
    if (flits > (int) (GlobalParams::buffer_depth * GlobalParams::dyad_threshold))
      return true;
  }
  return false;
}

void Router::ShowBuffersStats(std::ostream & out)
{
  for (int i=0; i<DIRECTIONS+1; i++)
    for (int vc=0; vc<GlobalParams::n_virtual_channels;vc++)
      buffer[i][vc].ShowStats(out);
}