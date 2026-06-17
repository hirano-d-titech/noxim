/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the implementaton of the global statistics
 */

#include "GlobalStats.h"
using namespace std;

GlobalStats::GlobalStats(const NoC * _noc)
{
    noc = _noc;

  #ifdef TESTING
    drained_total = 0;
  #endif
}

double GlobalStats::getAverageDelay()
{
    unsigned int total_packets = 0;
    double avg_delay = 0.0;

    if (GlobalParams::topology == TOPOLOGY_MESH)
    {
  for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
      for (int x = 0; x < GlobalParams::mesh_dim_x; x++) 
      {
    unsigned int received_packets =
        noc->t[x][y]->r->stats.getReceivedPackets();

    if (received_packets) 
    {
        avg_delay +=
      received_packets *
      noc->t[x][y]->r->stats.getAverageDelay();
        total_packets += received_packets;
    }
      }
    }
    else // other topology does not exist
    {
  assert(false);
    }

    avg_delay /= (double) total_packets;

    return avg_delay;
}



double GlobalStats::getAverageDelay(const int src_id,
           const int dst_id)
{
    Tile *tile = noc->searchNode(dst_id);

    assert(tile != NULL);

    return tile->r->stats.getAverageDelay(src_id);
}

double GlobalStats::getMaxDelay()
{
    double maxd = -1.0;

    if (GlobalParams::topology == TOPOLOGY_MESH) 
    {
  for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
      for (int x = 0; x < GlobalParams::mesh_dim_x; x++) 
      {
    Coord coord;
    coord.x = x;
    coord.y = y;
    int node_id = coord2Id(coord);
    double d = getMaxDelay(node_id);
    if (d > maxd)
        maxd = d;
      }

    }
    else // other topology does not exist
    {
  assert(false);
    }

    return maxd;
}

double GlobalStats::getMaxDelay(const int node_id)
{
  if (GlobalParams::topology == TOPOLOGY_MESH) 
  {
    Coord coord = id2Coord(node_id);

    unsigned int received_packets =
      noc->t[coord.x][coord.y]->r->stats.getReceivedPackets();

    if (received_packets)
      return noc->t[coord.x][coord.y]->r->stats.getMaxDelay();
    else
      return -1.0;
  }
  else // other topology does not exist
  {
    assert(false);
  }
}

double GlobalStats::getMaxDelay(const int src_id, const int dst_id)
{
  Tile *tile = noc->searchNode(dst_id);

  assert(tile != NULL);

  return tile->r->stats.getMaxDelay(src_id);
}

vector<vector<double>> GlobalStats::getMaxDelayMtx()
{
  vector<vector<double>> mtx;

  assert(GlobalParams::topology == TOPOLOGY_MESH); 

  mtx.resize(GlobalParams::mesh_dim_y);
  for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
    mtx[y].resize(GlobalParams::mesh_dim_x);

  for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
    for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
    {
        Coord coord;
        coord.x = x;
        coord.y = y;
        int id = coord2Id(coord);
        mtx[y][x] = getMaxDelay(id);
    }

  return mtx;
}

double GlobalStats::getAverageThroughput(const int src_id, const int dst_id)
{
  Tile *tile = noc->searchNode(dst_id);

  assert(tile != NULL);

  return tile->r->stats.getAverageThroughput(src_id);
}

/*
double GlobalStats::getAverageThroughput()
{
  unsigned int total_comms = 0;
  double avg_throughput = 0.0;

  for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
    for (int x = 0; x < GlobalParams::mesh_dim_x; x++) {
      unsigned int ncomms =
      noc->t[x][y]->r->stats.getTotalCommunications();

      if (ncomms) {
        avg_throughput += ncomms * noc->t[x][y]->r->stats.getAverageThroughput();
        total_comms += ncomms;
      }
    }

  avg_throughput /= (double) total_comms;

  return avg_throughput;
}
*/

double GlobalStats::getAggregatedThroughput()
{
  int total_cycles = GlobalParams::simulation_time - GlobalParams::stats_warm_up_time;

  return (double)getReceivedFlits()/(double)(total_cycles);
}

unsigned int GlobalStats::getReceivedPackets()
{
  unsigned int n = 0;

  if (GlobalParams::topology == TOPOLOGY_MESH) 
  {
    for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
      for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
        n += noc->t[x][y]->r->stats.getReceivedPackets();
  }
  else // other topology does not exist
  {
    assert(false);
  }

  return n;
}

unsigned int GlobalStats::getReceivedFlits()
{
  unsigned int n = 0;
  if (GlobalParams::topology == TOPOLOGY_MESH) 
  {
    for (int y = 0; y < GlobalParams::mesh_dim_y; y++) {
      for (int x = 0; x < GlobalParams::mesh_dim_x; x++) {
        n += noc->t[x][y]->r->stats.getReceivedFlits();
#ifdef TESTING
        drained_total += noc->t[x][y]->r->local_drained;
#endif
      }
    }
  }
  else // other topology does not exist
  {
    assert(false);
  }

  return n;
}

double GlobalStats::getThroughput()
{
  if (GlobalParams::topology == TOPOLOGY_MESH) 
  {
      int number_of_ip = GlobalParams::mesh_dim_x * GlobalParams::mesh_dim_y;
      return (double)getAggregatedThroughput()/(double)(number_of_ip);
  }
  else // other topology does not exist
  {
    assert(false);
  }
}

// Only accounting IP that received at least one flit
double GlobalStats::getActiveThroughput()
{
  int total_cycles = GlobalParams::simulation_time - GlobalParams::stats_warm_up_time;
  unsigned int n = 0;
  unsigned int trf = 0;
  unsigned int rf ;
  if (GlobalParams::topology == TOPOLOGY_MESH)
  {
    for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
    {
      for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
      {
        rf = noc->t[x][y]->r->stats.getReceivedFlits();

        if (rf != 0) n++;

        trf += rf;
      }
    }
  }
  else // other topology does not exist
  {
    assert(false);
  }

  return (double) trf / (double) (total_cycles * n);
}

vector < vector < unsigned long > > GlobalStats::getRoutedFlitsMtx()
{
  vector < vector < unsigned long > > mtx;
  assert (GlobalParams::topology == TOPOLOGY_MESH); 

  mtx.resize(GlobalParams::mesh_dim_y);
  for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
    mtx[y].resize(GlobalParams::mesh_dim_x);

  for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
    for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
      mtx[y][x] = noc->t[x][y]->r->getRoutedFlits();


  return mtx;
}

void GlobalStats::showStats(std::ostream & out, bool detailed)
{
  if (detailed) 
  {
    assert (GlobalParams::topology == TOPOLOGY_MESH); 
    out << endl << "detailed = [" << endl;

    for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
      for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
        noc->t[x][y]->r->stats.showStats(y * GlobalParams:: mesh_dim_x + x, out, true);
    out << "];" << endl;

    // show MaxDelay matrix
    vector < vector < double > > md_mtx = getMaxDelayMtx();

    out << endl << "max_delay = [" << endl;
    for (unsigned int y = 0; y < md_mtx.size(); y++) 
    {
      out << "   ";
      for (unsigned int x = 0; x < md_mtx[y].size(); x++)
        out << setw(6) << md_mtx[y][x];
      out << endl;
    }
    out << "];" << endl;

    // show RoutedFlits matrix
    vector < vector < unsigned long > > rf_mtx = getRoutedFlitsMtx();

    out << endl << "routed_flits = [" << endl;
    for (unsigned int y = 0; y < rf_mtx.size(); y++) 
    {
      out << "   ";
      for (unsigned int x = 0; x < rf_mtx[y].size(); x++)
        out << setw(10) << rf_mtx[y][x];
      out << endl;
    }
    out << "];" << endl;
  }

#ifdef DEBUG

  if (GlobalParams::topology == TOPOLOGY_MESH)
  {
    for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
      for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
        out << "PE["<<x << "," << y<< "]" << noc->t[x][y]->pe->getQueueSize()<< ",";
  }
  else // other topology does not exist
  {
    assert(false);
  }

  out << endl;
#endif

  out << "% Total received packets: " << getReceivedPackets() << endl;
  out << "% Failure/Decode packets Ratio: " << getFailureDecodeRatio() << endl;
  out << "% Error/Success packets Ratio: " << getErrorSuccessRatio() << endl;
  out << "% Total received flits: " << getReceivedFlits() << endl;
  out << "% Received/Ideal flits Ratio: " << getReceivedIdealFlitRatio() << endl;
  out << "% Global average retransmissions/Ideal flits: " << getAverageRetransmissions() << endl;
  out << "% Global average delay (cycles): " << getAverageDelay() << endl;
  out << "% Max delay (cycles): " << getMaxDelay() << endl;
  out << "% Network throughput (flits/cycle): " << getAggregatedThroughput() << endl;
  out << "% Average IP throughput (flits/cycle/IP): " << getThroughput() << endl;

  if (GlobalParams::show_buffer_stats)
    showBufferStats(out);

  if (GlobalParams::eval_cluster)
  {
    out << endl << "=========================================" << endl;
    out << "   CLUSTER EVALUATIONS BOARD PER NODE" << endl;
    out << "=========================================" << endl;

    int mesh_cx = (GlobalParams::mesh_dim_x + 1) / 2;
    int mesh_cy = (GlobalParams::mesh_dim_y + 1) / 2;
    int total_clusters = mesh_cx * mesh_cy;

    for (int c = 0; c < total_clusters; c++)
    {
      out << "Cluster " << c << " Evaluations Board:" << endl;
      for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
      {
        for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
        {
          double val = 0.0;
          auto it = noc->t[x][y]->pe->cluster_evaluations.find(c);
          if (it != noc->t[x][y]->pe->cluster_evaluations.end()) {
            val = it->second;
          }
          int int_val = static_cast<int>(val);
          if (int_val < -99) {
            out << " BAD";
          } else {
            out << " " << setw(3) << int_val;
          }
        }
        out << endl;
      }
      out << endl;
    }
  }
}

void GlobalStats::showBufferStats(std::ostream & out)
{
  out << "Router id\tBuffer N\t\tBuffer E\t\tBuffer S\t\tBuffer W\t\tBuffer L" << endl;
  out << "         \tMean\tMax\tMean\tMax\tMean\tMax\tMean\tMax\tMean\tMax" << endl;

  if (GlobalParams::topology == TOPOLOGY_MESH) 
  {
    for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
      for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
      {
        out << noc->t[x][y]->r->local_id;
        noc->t[x][y]->r->ShowBuffersStats(out);
        out << endl;
      }
  }
  else // other topology does not exist
  {
    assert(false);
  }

}

double GlobalStats::getReceivedIdealFlitRatio()
{
  int total_cycles;
  total_cycles= GlobalParams::simulation_time - GlobalParams::stats_warm_up_time;
  double ratio;
  if (GlobalParams::topology == TOPOLOGY_MESH) 
  {
    ratio = getReceivedFlits() /(GlobalParams::packet_injection_rate * (GlobalParams::min_packet_size +
            GlobalParams::max_packet_size)/2 * total_cycles * GlobalParams::mesh_dim_y * GlobalParams::mesh_dim_x);
  }
  else // other topology does not exist
  {
    assert(false);
  }
  return ratio;
}

double GlobalStats::getAverageRetransmissions()
{
  int total_cycles = GlobalParams::simulation_time - GlobalParams::stats_warm_up_time;
  double ideal_flits = GlobalParams::packet_injection_rate * (GlobalParams::min_packet_size +
              GlobalParams::max_packet_size)/2.0 * total_cycles * GlobalParams::mesh_dim_y * GlobalParams::mesh_dim_x;
  return (ideal_flits > 0.0) ? (double)GlobalParams::total_retransmissions / ideal_flits : 0.0;
}

double GlobalStats::getFailureDecodeRatio()
{
  auto ecm = EncodingModels::get(GlobalParams::encoding_model);
  unsigned int decode = ecm->getDecodeCount();
  return (decode > 0) ? (double)ecm->getFailureCount() / decode : 0.0;
}

double GlobalStats::getErrorSuccessRatio()
{
  auto ecm = EncodingModels::get(GlobalParams::encoding_model);
  unsigned int success = ecm->getDecodeCount() - ecm->getFailureCount();
  return (success > 0) ? (double)ecm->getErrorCount() / success : 0.0;
}
