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


inline int toggleKthBit(int n, int k)
{
	return (n ^ (1 << (k-1)));
}

void Router::process()
{
    txProcess();
    rxProcess();
}

void Router::rxProcess()
{
    if (reset.read()) {
	TBufferFullStatus bfs;
	// Clear outputs and indexes of receiving protocol
	for (int i = 0; i < DIRECTIONS + 2; i++) {
	    ack_rx[i].write(0);
	    current_level_rx[i] = 0;
	    buffer_full_status_rx[i].write(bfs);
	}
	routed_flits = 0;
	local_drained = 0;
    } 
    else 
    { 
	// This process simply sees a flow of incoming flits. All arbitration
	// and wormhole related issues are addressed in the txProcess()
	//assert(false);

	//Receive TSV flits 
	if (!tsv_link->tsv_buffer.empty()) {
		//Compare z-coords of flit destination and router
		int targetZ = id2Coord(tsv_link->tsv_buffer.front().dst_id).z;
		int thisZ = id2Coord(this->local_id).z;
		if (targetZ == thisZ) { 
			//Reception
			Flit received_flit = tsv_link->tsv_buffer.front();
			int vc = received_flit.vc_id;
			tsv_link->tsv_buffer.pop();
			//Recieve at terminal opposite the direction the flit travels
			if ((tsv_link->direction == DIRECTION_DOWN) && !buffer[DIRECTION_UP][vc].IsFull()) {
				buffer[DIRECTION_UP][vc].Push(received_flit);
			}
			else if ((tsv_link->direction == DIRECTION_UP) && !buffer[DIRECTION_DOWN][vc].IsFull()) {
				buffer[DIRECTION_DOWN][vc].Push(received_flit);
			}
			else {
				LOG << "tsv error" << endl;
			}
		}
	}

	for (int i = 0; i < DIRECTIONS + 2; i++) {
	    // To accept a new flit, the following conditions must match:
	    // 1) there is an incoming request
	    // 2) there is a free slot in the input buffer of direction i
	    //LOG<<"****RX****DIRECTION ="<<i<<  endl;

	    if (req_rx[i].read() == 1 - current_level_rx[i])
	    { 
		Flit received_flit = flit_rx[i].read();
		//LOG<<"request opposite to the current_level, reading flit "<<received_flit<<endl;

		int vc = received_flit.vc_id;

		if (!buffer[i][vc].IsFull()) 
		{

		    // Store the incoming flit in the circular buffer
		    buffer[i][vc].Push(received_flit);
		    LOG << " Flit " << received_flit << " collected from Input[" << i << "][" << vc <<"]" << endl;

            TRACEO << "Incoming Flit -> R_id: " << local_id << " , in_port: " << i << ", in_vc: " << vc << endl;

		    power.bufferRouterPush();

		    // Negate the old value for Alternating Bit Protocol (ABP)
		    //LOG<<"INVERTING CL FROM "<< current_level_rx[i]<< " TO "<<  1 - current_level_rx[i]<<endl;
		    current_level_rx[i] = 1 - current_level_rx[i];

		    // if a new flit is injected from local PE
		    if (received_flit.src_id == local_id)
			power.networkInterface();
		}

		else  // buffer full
		{
		    // should not happen with the new TBufferFullStatus control signals    
		    // except for flit coming from local PE, which don't use it 
		    LOG << " Flit " << received_flit << " buffer full Input[" << i << "][" << vc <<"]" << endl;
		    assert(i== DIRECTION_LOCAL);
		}

	    }
	    ack_rx[i].write(current_level_rx[i]);
	    // updates the mask of VCs to prevent incoming data on full buffers
	    TBufferFullStatus bfs;
	    for (int vc=0;vc<GlobalParams::n_virtual_channels;vc++)
		bfs.mask[vc] = buffer[i][vc].IsFull();
	    buffer_full_status_rx[i].write(bfs);
	}
    }
}

void Router::txProcess()
{
	//Possible placement of routing algorithm using buffers?
	if (aont_buffer.IsFull()) {
		Flit flit = aont_buffer.front();
		Coord src = id2Coord(flit.src_id);
		Coord dest = id2Coord(flit.dst_id);
		int mesh_dim_x = GlobalParams::mesh_dim_x;
		int mesh_dim_y = GlobalParams::mesh_dim_y;

		int bluex = 0;
		int bluey = 0;
		int redx = 0;
		int redy = 0;
		bool bluert;
		bool redrt;
		bool flipblue = false; //Flip routing algorithm on blue route (in edge case)
		//Normal case
		if ((src.x != dest.x) && (src.y != dest.y)) {
			//Boundary variables
			int bluetop = 0;
			int bluebot = 0;
			int bluelef = 0;
			int bluerig = 0;
			bool redtop = true; //True if top part of noc is red
			bool redleft = true; //True if left part of noc is red
			//Routing algorithms
			//By design routing algorithms apply to all cases
			bluert = false;
			redrt = true;
			//4 cases depending on where dest is with respect to src
			if (dest.x > src.x && dest.y > src.y) {
				bluetop = src.y + 1;
				bluebot = mesh_dim_y - 1;
				bluelef = 0;
				bluerig = dest.x - 1;
				redtop = true;
				redleft = false;
			}
			else if (dest.x > src.x && dest.y < src.y) {
				bluetop = 0;
				bluebot = src.y - 1;
				bluelef = 0;
				bluerig = dest.x - 1;
				redtop = false;
				redleft = false;
			}
			else if (dest.x < src.x && dest.y > src.y) {
				bluetop = src.y + 1;
				bluebot = mesh_dim_y - 1;
				bluelef = dest.x + 1;
				bluerig = mesh_dim_x - 1;
				redtop = true;
				redleft = true;
			}
			else {
				bluetop = 0;
				bluebot = src.y - 1;
				bluelef = dest.x + 1;
				bluerig = mesh_dim_x - 1;
				redtop = false;
				redleft = true;
			}

			//Random number generation
			srand(time(0));
			//(rand() % (max_value - min_value + 1)) + min_value;

			//Choose x and y coordinates for the blue node
			bluex = (rand() % (bluerig - bluelef + 1)) + bluelef;
			bluey = (rand() % (bluebot - bluetop + 1)) + bluetop;
			//Choose x and y coordinates for the red node
			//This is complex because there are two red rectangles
			//Find area and random coord from both rectangles
			int red1area = 0;
			int red1x = 0;
			int red1y = 0;
			int red2area = 0;
			int red2x = 0;
			int red2y = 0;
			if (redtop) {
				red1area = (src.y + 1) * mesh_dim_x;
				red1x = (rand() % (mesh_dim_x - 1 - 0 + 1)) + 0;
				red1y = (rand() % (src.y - 0 + 1)) + 0;
			}
			else {
				red1area = (mesh_dim_y - src.y) * mesh_dim_x;
				red1x = (rand() % (mesh_dim_x - 1 - 0 + 1)) + 0;
				red1y = (rand() % (mesh_dim_y - 1 - src.y + 1)) + 0;
			}
			if (redleft) {
				red2area = (dest.x + 1) * (bluebot - bluetop);
				red2x = (rand() % (dest.x - 0 + 1)) + 0;
				red2y = (rand() % (bluebot - bluetop + 1)) + bluetop;
			}
			else {
				red2area = (mesh_dim_x - dest.x) * (bluebot - bluetop);
				red2x = (rand() % (mesh_dim_x - 1 - dest.x + 1)) + dest.x;
				red2y = (rand() % (bluebot - bluetop + 1)) + bluetop;
			}
			//Randomly pick which rectangle to pick from
			int chosen = (rand() % ((red1area + red2area) - 1 + 1)) + 1;
			if (chosen <= red1area) {
				redx = red1x;
				redy = red1y;
			}
			else {
				redx = red2x;
				redy = red2y;
			}
		}
		//Src and dest lined up case
		else {
			//Horizontal line
			if (src.y == dest.y) {
				//Rare case: both on bottom edge, flip sides
				if (src.y == mesh_dim_y - 1) {
					redx = (random int between 0 and mesh_dim_x - 1, inclusive);
					redy = src.y;
					bluex = (random int between 0 and mesh_dim_x - 1, inclusive);
					bluey = (random int between 0 and src.y - 1, inclusive);
				}
				else {
					redx = (random int between 0 and mesh_dim_x - 1, inclusive);
					redy = (random int between 0 and src.y, inclusive);
					bluex = (random int between 0 and mesh_dim_x - 1, inclusive);
					bluey = (random int between src.y + 1 and mesh_dim_y - 1, inclusive);
				}
				bluert = false;
				redrt = false;
			}
			//Vertical line
			else {
				//Rare case: both on right edge, flip sides
				if (src.x == mesh_dim_x - 1) {
					redy = (random int between 0 and mesh_dim_y - 1, inclusive);
					redx = src.x;
					bluey = (random int between 0 and mesh_dim_y - 1, inclusive);
					bluex = (random int between 0 and src.x - 1, inclusive);
				}
				else {
					redy = (random int between 0 and mesh_dim_y - 1, inclusive);
					redx = (random int between 0 and src.x, inclusive);
					bluey = (random int between 0 and mesh_dim_y - 1, inclusive);
					bluex = (random int between src.x + 1 and mesh_dim_x - 1, inclusive);
				}
				bluert = true;
				redrt = true;
			}
			flipblue = true; //Must flip for lined up case
		}
		Flit red_flit;
		Flit blue_flit;
		int red_target = (redy * GlobalParams::mesh_dim_x) + redx;
		int blue_target = (bluey * GlobalParams::mesh_dim_x) + bluex;
		//Set original dests to final dest
		red_flit.fin_id = red_flit.dst_id;
		blue_flit.fin_id = blue_flit.dst_id;
		//Set temp dests
		red_flit.dst_id = red_target;
		blue_flit.dst_id = blue_target;
		//Set routing
		red_flit.route_xy = redrt;
		blue_flit.route_xy = bluert;
		//Set vc_id
		if (redrt)
			red_flit.vc_id = 0;
		else
			red_flit.vc_id = 1;
		if (bluert)
			blue_flit.vc_id = 0;
		else
			blue_flit.vc_id = 1;
		//Set flip
		if (flipblue)
			blue_flit.flip_route = true;
	}


  if (reset.read()) 
    {
      // Clear outputs and indexes of transmitting protocol
      for (int i = 0; i < DIRECTIONS + 2; i++) 
	{
	  req_tx[i].write(0);
	  current_level_tx[i] = 0;
	}
    } 
  else 
    { 
      // 1st phase: Reservation
      for (int j = 0; j < DIRECTIONS + 2; j++) 
	{
	  int i = (start_from_port + j) % (DIRECTIONS + 2);

	  for (int k = 0;k < GlobalParams::n_virtual_channels; k++)
	  {
	      int vc = (start_from_vc[i]+k)%(GlobalParams::n_virtual_channels);
	      
	      // Uncomment to enable deadlock checking on buffers. 
	      // Please also set the appropriate threshold.
	      // buffer[i].deadlockCheck();

	      if (!buffer[i][vc].IsEmpty()) 
	      {
		  Flit flit = buffer[i][vc].Front();
		  power.bufferRouterFront();

		  //Re-route flits if they reached the temporary destination
		  if (flit.dst_id == local_id && flit.dst_id != flit.fin_id) {
			flit.dst_id == flit.fin_id;
			//Flip routing algorithm if necessary
			if (flit.flip_route) {
				if (flit.route_xy) {
					flit.route_xy = false;
					flit.vc_id = 1;
				}
				else {
					flit.route_xy = true;
					flit.vc_id = 0;
				}
			}
		  }

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

		      // TODO: see PER POSTERI (adaptive routing should not recompute route if already reserved)
		      int o = route(route_data);

		      // manage special case of target hub not directly connected to destination
		      if (o>=DIRECTION_HUB_RELAY)
			  {
		      	Flit f = buffer[i][vc].Pop();
		      	f.hub_relay_node = o-DIRECTION_HUB_RELAY;
		      	buffer[i][vc].Push(f);
		      	o = DIRECTION_HUB;
			  }

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

      start_from_port = (start_from_port + 1) % (DIRECTIONS + 2);

      // 2nd phase: Forwarding
      //if (local_id==6) LOG<<"*TX*****local_id="<<local_id<<"__ack_tx[0]= "<<ack_tx[0].read()<<endl;
      for (int i = 0; i < DIRECTIONS + 2; i++) 
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
		  // power contribution already computed in 1st phase
		  Flit flit = buffer[i][vc].Front();
		  //LOG<< "*****TX***Direction= "<<i<< "************"<<endl;
		  //LOG<<"_cl_tx="<<current_level_tx[o]<<"req_tx="<<req_tx[o].read()<<" _ack= "<<ack_tx[o].read()<< endl;

		  //If direction is up or down, send to TSV 
		  if ((o == DIRECTION_DOWN) || (o == DIRECTION_UP)) {
			//Place in TSV_input buffer (to guarantee in order flit transmission)
			tsv_input_buffer.push(flit);
			buffer[i][vc].Pop();
			//Push direction info as well
			dir_queue.push(o);

			//Since reservation was made
			if (flit.flit_type == FLIT_TYPE_TAIL)
		      {
			  TReservation r;
			  r.input = i;
			  r.vc = vc;
			  reservation_table.release(r,o);
		      }
		  }
		  else {
		  
		  if ( (current_level_tx[o] == ack_tx[o].read()) &&
		       (buffer_full_status_tx[o].read().mask[vc] == false) ) 
		  {
		      //if (GlobalParams::verbose_mode > VERBOSE_OFF) 
		      LOG << "Input[" << i << "][" << vc << "] forwarded to Output[" << o << "], flit: " << flit << endl;

              TRACEO << "Outgoing Flit -> R_id: " << local_id << " , in_port: " << i << ", out_port: " << o << endl;

		      flit_tx[o].write(flit);
		      current_level_tx[o] = 1 - current_level_tx[o];
		      req_tx[o].write(current_level_tx[o]);
		      buffer[i][vc].Pop();

		      if (flit.flit_type == FLIT_TYPE_TAIL)
		      {
			  TReservation r;
			  r.input = i;
			  r.vc = vc;
			  reservation_table.release(r,o);
		      }

		      /* Power & Stats ------------------------------------------------- */
		      if (o == DIRECTION_HUB) power.r2hLink();
		      else
			  power.r2rLink();

		      power.bufferRouterPop();
		      power.crossBar();

		      if (o == DIRECTION_LOCAL) 
		      {
			  power.networkInterface();
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
		      /* End Power & Stats ------------------------------------------------- */
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
	      }
	  } // if not reserved 
	 // else LOG<<"we have no reservation for direction "<<i<< endl;
      } // for loop directions

	  //Once per cycle, if tsv_input_buffer has items, try to gain access of bus 
		if ((!tsv_input_buffer.empty()) && (tsv_link->reqAccess(local_id))) {
			//Push first flit to bus
        	tsv_link->tsv_buffer.push(tsv_input_buffer.front());
			tsv_input_buffer.pop();
			//Set direction based on queue
			tsv_link->direction = dir_queue.front();
			dir_queue.pop();
    	} 

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

void Router::perCycleUpdate()
{
    if (reset.read()) {
	for (int i = 0; i < DIRECTIONS + 1; i++)
	    free_slots[i].write(buffer[i][DEFAULT_VC].GetMaxBufferSize());
    } else {
        selectionStrategy->perCycleUpdate(this);

	power.leakageRouter();
	for (int i = 0; i < DIRECTIONS + 1; i++)
	{
	    for (int vc=0;vc<GlobalParams::n_virtual_channels;vc++)
	    {
		power.leakageBufferRouter();
		power.leakageLinkRouter2Router();
	    }
	}

	power.leakageLinkRouter2Hub();
    }
}

vector<int> Router::nextDeltaHops(RouteData rd) {

	if (GlobalParams::topology == TOPOLOGY_MESH)
	{
		cout << "Mesh topologies are not supported for nextDeltaHops() ";
		assert(false);
	}
	// annotate the initial nodes
	int src = rd.src_id;
	int dst = rd.dst_id;

	int current_node = src;
	vector<int> direction; // initially is empty
	vector<int> next_hops;

	int sw = GlobalParams::n_delta_tiles/2; //sw: switch number in each stage
	int stg = log2(GlobalParams::n_delta_tiles);
	int c;
	//---From Source to stage 0 (return the sw attached to the source)---
	//Topology omega 
	if (GlobalParams::topology == TOPOLOGY_OMEGA) 	
	{
	if(current_node < (GlobalParams::n_delta_tiles/2))	
		 c = current_node;
	else if(current_node >= (GlobalParams::n_delta_tiles/2))	
		 c = (current_node - (GlobalParams::n_delta_tiles/2));		
	}
	//Other delta topologies: Butterfly and baseline
	else if ((GlobalParams::topology == TOPOLOGY_BUTTERFLY)||(GlobalParams::topology == TOPOLOGY_BASELINE))
	{
		 c =  (current_node >>1);
	}

		Coord temp_coord;
		temp_coord.x = 0;
		temp_coord.y = c;
		int N = coord2Id(temp_coord);

		next_hops.push_back(N);
		current_node = N;
	
	
   //---From stage 0 to Destination---
	int current_stage = 0;

	while (current_stage<stg-1)
	{
		Coord new_coord;
		int y = id2Coord(current_node).y;

		rd.current_id = current_node;
		direction = routingAlgorithm->route(this, rd);

		int bit_to_check = stg - current_stage - 1;

		int bit_checked = (y & (1 << (bit_to_check - 1)))>0 ? 1:0;

		// computes next node coords
		new_coord.x = current_stage + 1;
		if (bit_checked ^ direction[0])
			new_coord.y = toggleKthBit(y, bit_to_check);
		else
			new_coord.y = y;

		current_node = coord2Id(new_coord);
		next_hops.push_back(current_node);
		current_stage = id2Coord(current_node).x;
	}

	next_hops.push_back(dst);

	return next_hops;

}

vector < int > Router::routingFunction(const RouteData & route_data)
{
	if (GlobalParams::use_winoc)
	{
		// - If the current node C and the destination D are connected to an radiohub, use wireless
		// - If D is not directly connected to a radio hub, wireless
		// communication can still  be used if some intermediate node "I" in the routing
		// path is reachable from current node C.
		// - Since further wired hops will be required from I -> D, a threshold "winoc_dst_hops"
		// can be specified (via command line) to determine the max distance from the intermediate
		// node I and the destination D.
		// - NOTE: default threshold is 0, which means I=D, i.e., we explicitly ask the destination D to be connected to the
		// target radio hub
		if (hasRadioHub(local_id))
		{
			// Check if destination is directly connected to an hub
			if ( hasRadioHub(route_data.dst_id) &&
				 !sameRadioHub(local_id,route_data.dst_id) )
			{
                map<int, int>::iterator it1 = GlobalParams::hub_for_tile.find(route_data.dst_id);
                map<int, int>::iterator it2 = GlobalParams::hub_for_tile.find(route_data.current_id);

                if (connectedHubs(it1->second,it2->second))
                {
                    LOG << "Destination node " << route_data.dst_id << " is directly connected to a reachable RadioHub" << endl;
                    vector<int> dirv;
                    dirv.push_back(DIRECTION_HUB);
                    return dirv;
                }
			}
			// let's check whether some node in the route has an acceptable distance to the dst
            if (GlobalParams::winoc_dst_hops>0)
            {
                // TODO: for the moment, just print the set of nexts hops to check everything is ok
                LOG << "NEXT_DELTA_HOPS (from node " << route_data.src_id << " to " << route_data.dst_id << ") >>>> :";
                vector<int> nexthops;
                nexthops = nextDeltaHops(route_data);
                //for (int i=0;i<nexthops.size();i++) cout << "(" << nexthops[i] <<")-->";
                //cout << endl;
                for (int i=1;i<=GlobalParams::winoc_dst_hops;i++)
				{
                	int dest_position = nexthops.size()-1;
                	int candidate_hop = nexthops[dest_position-i];
					if ( hasRadioHub(candidate_hop) && !sameRadioHub(local_id,candidate_hop) ) {
						//LOG << "Checking candidate hop " << candidate_hop << " ... It's OK!" << endl;
						LOG << "Relaying to hub-connected node " << candidate_hop << " to reach destination " << route_data.dst_id << endl;
						vector<int> dirv;
						dirv.push_back(DIRECTION_HUB_RELAY+candidate_hop);
						return dirv;
					}
					//else
					// LOG << "Checking candidate hop " << candidate_hop << " ... NOT OK" << endl;
				}
            }
		}
	}
	// TODO: fix all the deprecated verbose mode logs
	if (GlobalParams::verbose_mode > VERBOSE_OFF)
		LOG << "Wired routing for dst = " << route_data.dst_id << endl;

	// not wireless direction taken, apply normal routing
	return routingAlgorithm->route(this, route_data);
}

int Router::route(const RouteData & route_data)
{

    if (route_data.dst_id == local_id)
	return DIRECTION_LOCAL;

    power.routing();
    vector < int >candidate_channels = routingFunction(route_data);

    power.selection();
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

    for (unsigned int i = 0; i < nop_channels.size(); i++) {
	int available;

	if (nop_data.channel_status_neighbor[nop_channels[i]].available)
	    available = 1;
	else
	    available = 0;

	int free_slots =
	    nop_data.channel_status_neighbor[nop_channels[i]].free_slots;

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
    stats.configure(_id, _warm_up_time);

    start_from_port = DIRECTION_LOCAL;
  

    if (grt.isValid())
	routing_table.configure(grt, _id);

    reservation_table.setSize(DIRECTIONS+2);

    for (int i = 0; i < DIRECTIONS + 2; i++)
    {
	for (int vc = 0; vc < GlobalParams::n_virtual_channels; vc++)
	{
	    buffer[i][vc].SetMaxBufferSize(_max_buffer_size);
	    buffer[i][vc].setLabel(string(name())+"->buffer["+i_to_string(i)+"]");
	}
	start_from_vc[i] = 0;
    }


    if (GlobalParams::topology == TOPOLOGY_MESH)
    {
	int layer = _id / (GlobalParams::mesh_dim_x * GlobalParams::mesh_dim_y);
    int temp = _id % (GlobalParams::mesh_dim_x * GlobalParams::mesh_dim_y);
    int col = temp / GlobalParams::mesh_dim_x;
    int row = temp % GlobalParams::mesh_dim_x;

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
		if (layer == 0)
	      buffer[DIRECTION_UP][vc].Disable();
	    if (layer == GlobalParams::mesh_dim_z-1)
	      buffer[DIRECTION_DOWN][vc].Disable();
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
	if (direction == DIRECTION_UP)
	return DIRECTION_DOWN;
    if (direction == DIRECTION_DOWN)
	return DIRECTION_UP;

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
	case DIRECTION_UP:
	if (my_coord.z == 0)
	    return NOT_VALID;
	my_coord.z--;
	break;
    case DIRECTION_DOWN:
	if (my_coord.z == GlobalParams::mesh_dim_z - 1)
	    return NOT_VALID;
	my_coord.z++;
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
    for (int i = 0; i < DIRECTIONS; i++) {

	if (free_slots_neighbor[i]==NOT_VALID) continue;

	int flits = GlobalParams::buffer_depth - free_slots_neighbor[i];
	if (flits > (int) (GlobalParams::buffer_depth * GlobalParams::dyad_threshold))
	    return true;
    }

    return false;
}

void Router::ShowBuffersStats(std::ostream & out)
{
  for (int i=0; i<DIRECTIONS+2; i++)
      for (int vc=0; vc<GlobalParams::n_virtual_channels;vc++)
	    buffer[i][vc].ShowStats(out);
}


bool Router::connectedHubs(int src_hub, int dst_hub) {
    vector<int> &first = GlobalParams::hub_configuration[src_hub].txChannels;
    vector<int> &second = GlobalParams::hub_configuration[dst_hub].rxChannels;

    vector<int> intersection;

    for (unsigned int i = 0; i < first.size(); i++) {
        for (unsigned int j = 0; j < second.size(); j++) {
            if (first[i] == second[j])
                intersection.push_back(first[i]);
        }
    }

    if (intersection.size() == 0)
        return false;
    else
        return true;
}

void Router::setTSV(TSV* tsv) {
	tsv_link = tsv;
}