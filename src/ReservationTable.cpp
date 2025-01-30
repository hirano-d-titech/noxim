/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the implementation of the switch reservation table
 */

#include "ReservationTable.h"

ReservationTable::ReservationTable()
{
}

void ReservationTable::setSize(const int n_outputs)
{
    this->n_outputs = n_outputs;
    rtable = new TRTEntry[this->n_outputs];

    for (int i=0;i<this->n_outputs;i++)
    {
	rtable[i].index = 0;
	rtable[i].reservations.clear();
	rtable[i].nc_enabled = false;
    }
}

bool ReservationTable::isNotReserved(const int port_out)
{
    assert(port_out<n_outputs);
    return (rtable[port_out].reservations.size()==0);
}

/* For a given input, returns the set of output/vc reserved from that input.
 * An index is required for each output entry, to avoid that multiple invokations
 * with different inputs returns the same output in the same clock cycle. */
vector<pair<int,int> > ReservationTable::getReservationsFrom(const int port_in)
{
    vector<pair<int,int> > reservations;

    for (int o = 0;o<n_outputs;o++)
    {
	if (rtable[o].reservations.size()>0)
	{
	    int current_index = rtable[o].index;
	    if (rtable[o].reservations[current_index].input == port_in)
		reservations.push_back(pair<int,int>(o,rtable[o].reservations[current_index].vc));
	}
    }
    return reservations;
}

pair<size_t, bool> ReservationTable::getReservationStatusTo(const int port_out)
{
	return {rtable[port_out].reservations.size(), rtable[port_out].nc_enabled};
}

vector<TReservation> ReservationTable::getReservationsTo(const int port_out)
{
    vector<TReservation> reservations;

	switch (rtable[port_out].reservations.size())
	{
		case 0:
			break;
		case 1:
			reservations.push_back(rtable[port_out].reservations[0]);
			break;
		default:
			if (rtable[port_out].nc_enabled) {
				for (auto &&trt : rtable[port_out].reservations)
				{
					reservations.push_back(trt);
				}
			} else {
				reservations.push_back(rtable[port_out].reservations[rtable[port_out].index]);
			}
		break;
	}

	return reservations;
}

FlitMetadata ReservationTable::getInitialFlitMetadataTo(const int port_out)
{
	return rtable[port_out].head_meta;
}

int ReservationTable::checkReservation(const TReservation r, const int port_out)
{
	// RT_ALREADY_OUT_OTHER is valid situation in Network Coding.
	if (GlobalParams::network_coding_type == NC_TYPE_NONE)
	{
    /* Sanity Check for forbidden table status:
     * - same input/VC in a different output line */
	for (int o=0;o<n_outputs;o++)
	{
		for (vector<TReservation>::size_type i=0;i<rtable[o].reservations.size(); i++)
		{
			if (o == port_out)
			{
				// the same VC for that output has been reserved by another input
				if (rtable[port_out].reservations[i].input != r.input &&
					rtable[port_out].reservations[i].vc == r.vc)
					return RT_OUTVC_BUSY;
			}
			else if (rtable[o].reservations[i] == r)
			{
				// In the current implementation this should never happen
				return RT_ALREADY_OTHER_OUT;
			}
		}
	}
	}
	else /* GlobalParams::network_coding_type != NC_TYPE_NONE */
	{
	// if addition of multiplicity exceed MAX_NC_META, it cant be merged...
	if (rtable[port_out].out_multiplicity + r.mult > NCHistory::MAX_META)
	{
		return RT_OUTVC_BUSY;
	}
	}

    /* On a given output entry, reservations must differ by VC
     *  Motivation: they will be interleaved cycle-by-cycle as index moves */
	int n_reservations = rtable[port_out].reservations.size();
	for (int i=0;i< n_reservations; i++)
	{
		// the reservation is already present
		if (rtable[port_out].reservations[i] == r)
			return RT_ALREADY_SAME;
	}

	if (rtable[port_out].reservations.size() > 0)
	{
	// do not encode next at local
	if (port_out == DIRECTION_LOCAL || rtable[port_out].head_meta.nc_state != NC_NORMAL)
	{
	return RT_OUTVC_BUSY;
	}
	// available if exporting with network coding
	return RT_ENCODABLE;
	}
	else
	{
	return RT_AVAILABLE;
	}
}

void ReservationTable::print()
{
    for (int o=0;o<n_outputs;o++)
    {
	cout << o << ": ";
	for (vector<TReservation>::size_type i=0;i<rtable[o].reservations.size();i++)
	{
	    cout << "<" << rtable[o].reservations[i].input << "," << rtable[o].reservations[i].vc << ">, ";
	}
	cout << " | " << rtable[o].index;
	cout << endl;
    }
}


void ReservationTable::reserve(const TReservation r, FlitMetadata meta, const int port_out)
{
    // IMPORTANT: problem when used by Hub with more connections
    //
    // reservation of reserved/not valid ports is illegal. Correctness
    // should be assured by ReservationTable users
	auto status = checkReservation(r, port_out);
    assert(reservable(status));

	if (GlobalParams::network_coding_type != NC_TYPE_NONE)
	{
		if (rtable[port_out].reservations.size() == 0) {
			// save the first flit metadata for network coding
			rtable[port_out].head_meta = meta;
			rtable[port_out].head_meta.hop_no = 0;
			rtable[port_out].head_meta.hub_hop_no = 0;
		} else if (!rtable[port_out].nc_enabled) {
			// enable network coding
			assert(rtable[port_out].reservations.size() == 1);
			rtable[port_out].nc_enabled = true;
			rtable[port_out].head_meta.nc_state = NC_MULTIC;
			rtable[port_out].head_meta.timestamp = sc_time_stamp().to_double();
		} else {
			// nothing to do
		}
		rtable[port_out].reservations.push_back(r);
	}
	else
	{
		// TODO: a better policy could insert in a specific position as far a possible
		// from the current index
		rtable[port_out].reservations.push_back(r);
	}
	
	rtable[port_out].out_multiplicity += r.mult;
}

void ReservationTable::release(const TReservation r, const int port_out)
{
    assert(port_out < n_outputs);

    for (vector<TReservation>::iterator i=rtable[port_out].reservations.begin(); 
	    i != rtable[port_out].reservations.end(); i++)
    {
	if (*i == r)
	{
		rtable[port_out].out_multiplicity -= i->mult;
	    rtable[port_out].reservations.erase(i);
	    vector<TReservation>::size_type removed_index = i - rtable[port_out].reservations.begin();

	    if (removed_index < rtable[port_out].index){
		rtable[port_out].index--;
		}
	    else{
		if (rtable[port_out].index >= rtable[port_out].reservations.size())
		    rtable[port_out].index = 0;
		}

		/**
		 * If the reservation table is empty, disable network coding
		 * Flits must remain in order, so if one or more reservation is present, it must be keeped
		 */
		if (rtable[port_out].reservations.size() == 0)
		{
			rtable[port_out].nc_enabled = false;
			rtable[port_out].head_meta = FlitMetadata{};
		}

	    return;
	}
    }
    assert(false); //trying to release a never made reservation  ?
}

void ReservationTable::updateIndex()
{
    for (int o=0;o<n_outputs;o++)
    {
	if (rtable[o].reservations.size()>0)
	{
		rtable[o].index = (rtable[o].index+1)%(rtable[o].reservations.size());
		rtable[o].head_meta.sequence_no++;
	}
	}
}


