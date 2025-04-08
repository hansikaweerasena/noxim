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
    return min +
	(int) ((double) (max - min + 1) * rand() / (RAND_MAX + 1.0));
}

void ProcessingElement::rxProcess()
{
    if (reset.read()) {
	ack_rx.write(0);
	current_level_rx = 0;
    } else {
	if (req_rx.read() == 1 - current_level_rx) {
	    Flit flit_tmp = flit_rx.read();
        if ((GlobalParams::traffic_distribution == TRAFFIC_TRACE_BASED || GlobalParams::traffic_distribution == TRAFFIC_HYBRID_TAB_TRA) && flit_tmp.flit_type == FLIT_TYPE_TAIL) {
            if (flit_tmp.trace_id >= 0) {
                injectFuturePackets(flit_tmp);
                LOG << "*** [des" << flit_tmp.dst_id << "] from " << flit_tmp.src_id << ", src" << flit_tmp << endl;
            } else if (flit_tmp.payload_type == DOS) {
                FuturePacket future_packet;
                int vc = randInt(0,GlobalParams::n_virtual_channels-1);
                future_packet.packet.make(flit_tmp.dst_id, flit_tmp.src_id, vc, -1, 5);
                future_packet.packet.trace_id = -1;
                future_packet.packet.addr = flit_tmp.addr;
                future_packet.packet.payload_type = OTHER;
                double now = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
                future_packet.injection_cycle = now + 70;    // memory access time 70 cycles
                future_packets.push(future_packet);
            }
        }
	    current_level_rx = 1 - current_level_rx;	// Negate the old value for Alternating Bit Protocol (ABP)
	}
	ack_rx.write(current_level_rx);
    }
}

void ProcessingElement::initTraceInjector(GlobalTraceInjector& global_trace_injector)
{
    if (GlobalParams::traffic_distribution == TRAFFIC_TRACE_BASED || GlobalParams::traffic_distribution == TRAFFIC_HYBRID_TAB_TRA) {
        trace_injector = &global_trace_injector;
        std::vector<FirstInMsg> first_in_msgs = trace_injector->getFirstInMsgs(local_id);
        // Iterate through the first_in_msgs and convert it to packets and inject the packets to the packets queue
        for (size_t i = 0; i < first_in_msgs.size(); i++) {
            FuturePacket future_packet;
            int vc = randInt(0,GlobalParams::n_virtual_channels-1);
            future_packet.packet.make(first_in_msgs[i].in_msg.src, first_in_msgs[i].in_msg.dest, vc, -1, first_in_msgs[i].in_msg.size);
            future_packet.packet.trace_id = first_in_msgs[i].trace_id;
            future_packet.packet.addr = first_in_msgs[i].in_msg.addr;
            if (first_in_msgs[i].in_msg.type == "EXCLUSIVE_UNBLOCK") {
                future_packet.packet.payload_type = EXCLUSIVE_UNBLOCK;
            } else {
                future_packet.packet.payload_type = OTHER;
            }
            future_packet.injection_cycle = GlobalParams::reset_time;
            future_packets.push(future_packet);
        }
    }
}

// Inject future packets to the future_packets queue based on the out tail flit
void ProcessingElement::injectFuturePackets(const Flit & out_flit){
    if (out_flit.payload_type != EXCLUSIVE_UNBLOCK) {
        try {
            Record nextRecord = trace_injector->getNextRecord(out_flit.trace_id, out_flit.src_id, out_flit.dst_id, out_flit.addr);
            FuturePacket future_packet;
            int vc = randInt(0,GlobalParams::n_virtual_channels-1);
            future_packet.packet.make(nextRecord.in_msg.src, nextRecord.in_msg.dest, vc, -1, nextRecord.in_msg.size);
            future_packet.packet.trace_id = out_flit.trace_id;
            future_packet.packet.addr = nextRecord.in_msg.addr;
            if (nextRecord.in_msg.type == "EXCLUSIVE_UNBLOCK") {
                future_packet.packet.payload_type = EXCLUSIVE_UNBLOCK;
            } else {
                future_packet.packet.payload_type = OTHER;
            }
            double now = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
            future_packet.injection_cycle = now + nextRecord.delay;
            future_packets.push(future_packet);

            // If in packet is EXCLUSIVE_UNBLOCK, then we need to inject the next packet in the same cycle
            if (nextRecord.in_msg.type == "EXCLUSIVE_UNBLOCK") {
                injectFuturePackets(out_flit);
            } 

            } catch (const std::runtime_error& e) {
                std::string errorMessage = e.what();
                if (errorMessage == "Queue is empty.") {
                    LOG << "Queue" << out_flit.trace_id << "is empty." << std::endl;
                } else {
                    throw e;
                }
            }
    }
}


void ProcessingElement::txProcess()
{
    if (reset.read()) {
	req_tx.write(0);
	current_level_tx = 0;
	transmittedAtPreviousCycle = false;
    } else {
    //Double packet generation with AONT 
	Packet packet1;
    Packet packet2;

	if (canShot(packet1, packet2)) {
	    packet_queue.push(packet1);
        packet_queue.push(packet2);
	    transmittedAtPreviousCycle = true;
	} else
	    transmittedAtPreviousCycle = false;


	if (ack_tx.read() == current_level_tx) {
	    if (!packet_queue.empty()) {
		Flit flit = nextFlit();	// Generate a new flit
		flit_tx->write(flit);	// Send the generated flit
		current_level_tx = 1 - current_level_tx;	// Negate the old value for Alternating Bit Protocol (ABP)
		req_tx.write(current_level_tx);
	    }
	}
    }
}

Flit ProcessingElement::nextFlit()
{
    Flit flit;
    Packet packet = packet_queue.front();

    flit.src_id = packet.src_id;
    flit.dst_id = packet.dst_id;
    flit.fin_id = packet.fin_id;
    flit.route_xy = packet.route_xy;
    flit.flip_route = packet.flip_route;
    flit.vc_id = packet.vc_id;
    flit.timestamp = packet.timestamp;
    flit.sequence_no = packet.size - packet.flit_left;
    flit.sequence_length = packet.size;
    flit.hop_no = 0;
    //  flit.payload     = DEFAULT_PAYLOAD;

    flit.payload_type = packet.payload_type;
    flit.addr = packet.addr;
    flit.trace_id = packet.trace_id;

    flit.hub_relay_node = NOT_VALID;

    if (packet.size == packet.flit_left)
	flit.flit_type = FLIT_TYPE_HEAD;
    else if (packet.flit_left == 1)
	flit.flit_type = FLIT_TYPE_TAIL;
    else
	flit.flit_type = FLIT_TYPE_BODY;

    packet_queue.front().flit_left--;
    if (packet_queue.front().flit_left == 0)
	packet_queue.pop();

    return flit;
}

bool ProcessingElement::canShot(Packet & red_packet, Packet & blue_packet)
{
   // assert(false);

//    check for all nodes except for one in table (otherwise the trace ones are avoided)
//    if(never_transmit) return false;
   
    //if(local_id!=16) return false;
    /* DEADLOCK TEST 
	double current_time = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;

	if (current_time >= 4100) 
	{
	    //if (current_time==3500)
	         //cout << name() << " IN CODA " << packet_queue.size() << endl;
	    return false;
	}
	//*/

#ifdef DEADLOCK_AVOIDANCE
    if (local_id%2==0)
	return false;
#endif
    bool shot;
    double threshold;

    double now = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;

    if (GlobalParams::traffic_distribution != TRAFFIC_TABLE_BASED && GlobalParams::traffic_distribution != TRAFFIC_TRACE_BASED && GlobalParams::traffic_distribution != TRAFFIC_HYBRID_TAB_TRA) {
	if (!transmittedAtPreviousCycle)
	    threshold = GlobalParams::packet_injection_rate;
	else
	    threshold = GlobalParams::probability_of_retransmission;

	shot = (((double) rand()) / RAND_MAX < threshold);
	if (shot) {
	    if (GlobalParams::traffic_distribution == TRAFFIC_RANDOM)
		    red_packet = trafficRandom();
        else if (GlobalParams::traffic_distribution == TRAFFIC_TRANSPOSE1)
		    red_packet = trafficTranspose1();
        else if (GlobalParams::traffic_distribution == TRAFFIC_TRANSPOSE2)
    		red_packet = trafficTranspose2();
        else if (GlobalParams::traffic_distribution == TRAFFIC_BIT_REVERSAL)
		    red_packet = trafficBitReversal();
        else if (GlobalParams::traffic_distribution == TRAFFIC_SHUFFLE)
		    red_packet = trafficShuffle();
        else if (GlobalParams::traffic_distribution == TRAFFIC_BUTTERFLY)
		    red_packet = trafficButterfly();
        else if (GlobalParams::traffic_distribution == TRAFFIC_LOCAL)
		    red_packet = trafficLocal();
        else if (GlobalParams::traffic_distribution == TRAFFIC_ULOCAL)
		    red_packet = trafficULocal();
        else {
            cout << "Invalid traffic distribution: " << GlobalParams::traffic_distribution << endl;
            exit(-1);
        }
	}
    } else if (GlobalParams::traffic_distribution == TRAFFIC_TABLE_BASED) {			// Table based communication traffic
        if (never_transmit)
            return false;

        bool use_pir = (transmittedAtPreviousCycle == false);
        vector < pair < int, double > > dst_prob;
        double threshold =
            traffic_table->getCumulativePirPor(local_id, (int) now, use_pir, dst_prob);

        double prob = (double) rand() / RAND_MAX;
        shot = (prob < threshold);
        if (shot) {
            for (unsigned int i = 0; i < dst_prob.size(); i++) {
            if (prob < dst_prob[i].second) {

                //Execute AONT
                int total_size = getRandomSize();


                Coord src = id2Coord(local_id);
                Coord dest = id2Coord(dst_prob[i].first);
                int mesh_dim_x = GlobalParams::mesh_dim_x;
                int mesh_dim_y = GlobalParams::mesh_dim_y;

                int bluex, bluey, redx, redy;
                bool bluert, redrt;
                bool flipblue = false; //Flip routing algorithm on blue route (in edge case)
                //Normal case
                if ((src.x != dest.x) && (src.y != dest.y)) {
                    //Boundary variables
                    int bluetop, bluebot, bluelef, bluerig;
                    bool redtop, redleft; //True if left or top part of noc is red
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

                    //Choose x and y coordinates for the blue node
                    bluex = randInt(bluelef, bluerig);
                    bluey = randInt(bluetop, bluebot);
                    //Choose x and y coordinates for the red node
                    //This is complex because there are two red rectangles
                    //Find area and random coord from both rectangles
                    int red1area, red1x, red1y, red2area, red2x, red2y;
                    if (redtop) 
                        red1area = (src.y + 1) * mesh_dim_x;
                    else 
                        red1area = (mesh_dim_y - src.y) * mesh_dim_x;
                    if (redleft) 
                        red2area = (dest.x + 1) * (bluebot - bluetop);
                    else 
                        red2area = (mesh_dim_x - dest.x) * (bluebot - bluetop);
                    //Randomly pick which rectangle to pick from
                    int chosen = randInt(1, red1area + red2area);
                    if (chosen <= red1area) {
                        if (redtop) {
                            redx = randInt(0, mesh_dim_x - 1);
                            redy = randInt(0, src.y);
                        }
                        else {
                            redx = randInt(0, mesh_dim_x - 1);
                            redy = randInt(src.y, mesh_dim_y - 1);
                        }
                    }
                    else {
                        if (redleft) {
                            redx = randInt(0, dest.x);
                            redy = randInt(bluetop, bluebot);
                        }
                        else {
                            redx = randInt(dest.x, mesh_dim_x - 1);
                            redy = randInt(bluetop, bluebot);
                        }
                    }
                }
                //Src and dest lined up case
                else {
                    //Horizontal line
                    if (src.y == dest.y) {
                        //Rare case: both on bottom edge, flip sides
                        if (src.y == mesh_dim_y - 1) {
                            redx = randInt(0, mesh_dim_x - 1);
                            redy = src.y;
                            bluex = randInt(0, mesh_dim_x - 1);
                            bluey = randInt(0, src.y - 1);
                        }
                        else {
                            redx = randInt(0, mesh_dim_x - 1);
                            redy = randInt(0, src.y);
                            bluex = randInt(0, mesh_dim_x - 1);
                            bluey = randInt(src.y + 1, mesh_dim_y - 1);
                        }
                        bluert = false;
                        redrt = false;
                    }
                    //Vertical line
                    else {
                        //Rare case: both on right edge, flip sides
                        if (src.x == mesh_dim_x - 1) {
                            redy = randInt(0, mesh_dim_y - 1);
                            redx = src.x;
                            bluey = randInt(0, mesh_dim_y - 1);
                            bluex = randInt(0, src.x - 1);
                        }
                        else {
                            redy = randInt(0, mesh_dim_y - 1);
                            redx = randInt(0, src.x);
                            bluey = randInt(0, mesh_dim_y - 1);
                            bluex = randInt(src.x + 1, mesh_dim_x - 1);
                        }
                        bluert = true;
                        redrt = true;
                    }
                    flipblue = true; //Must flip for lined up case
                }
                //Set vc_id
                int rvc, bvc;
                if (redrt)
                    rvc = 0;
                else
                    rvc = 1;
                if (bluert)
                    bvc = 0;
                else
                    bvc = 1;
                //Calculate targets 
                int red_target = (redy * GlobalParams::mesh_dim_x) + redx;
                int blue_target = (bluey * GlobalParams::mesh_dim_x) + bluex;
                //Set packet information
                red_packet.make(local_id, red_target, rvc, now, total_size / 2);
                blue_packet.make(local_id, blue_target, bvc, now, total_size / 2);
                red_packet.fin_id = dst_prob[i].first;
                blue_packet.fin_id = dst_prob[i].first;
                red_packet.route_xy = redrt;
                blue_packet.route_xy = bluert;
                if (flipblue)
                    blue_packet.flip_route = true;
                red_packet.trace_id = -1;
                blue_packet.trace_id = -1;
                break;
            }
            }
        }
    } else if (GlobalParams::traffic_distribution == TRAFFIC_TRACE_BASED) {	 //Trace based communication traffic
        if(!future_packets.empty()){
            FuturePacket future_packet = future_packets.front();
            if (future_packet.injection_cycle <= now) {
                red_packet = future_packet.packet;
                red_packet.timestamp = now;
                future_packets.pop();
                shot = true;
            }else{
                 shot = false;
            }
        }else{
            shot = false;
        }
    } else {  // Hybrid (Table based + Trace based) communication traffic
        if(!future_packets.empty()){
            FuturePacket future_packet = future_packets.front();
            if (future_packet.injection_cycle <= now) {
                red_packet = future_packet.packet;
                red_packet.timestamp = now;
                future_packets.pop();
                shot = true;
            }else{
                shot = false;
            }
        }else{
            shot = false;
        }

        if (shot == false){
            if (never_transmit)
                return false;

            bool use_pir = (transmittedAtPreviousCycle == false);
            vector < pair < int, double > > dst_prob;
            double threshold =
                    traffic_table->getCumulativePirPor(local_id, (int) now, use_pir, dst_prob);

            double prob = (double) rand() / RAND_MAX;
            shot = (prob < threshold);
            if (shot) {
                for (unsigned int i = 0; i < dst_prob.size(); i++) {
                    if (prob < dst_prob[i].second) {
                        int vc = randInt(0,GlobalParams::n_virtual_channels-1);
                        red_packet.make(local_id, dst_prob[i].first, vc, now, 2);   // All the table based packet are control packet with size 2
                        red_packet.trace_id = -1;
                        red_packet.payload_type = DOS;
                        break;
                    }
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

    int max_id = (GlobalParams::mesh_dim_x * GlobalParams::mesh_dim_y * GlobalParams::mesh_dim_z);

    for (int i=0;i<max_id;i++)
    {
	if (rnd<=GlobalParams::locality)
	{
	    if (local_id!=i && sameRadioHub(local_id,i))
		dst_set.push_back(i);
	}
	else
	    if (!sameRadioHub(local_id,i))
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
    int inc_z = rand()%2?-1:1;
    
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

    if (current.z==0)
	    if (inc_z<0) inc_z=0;

	if (current.z==GlobalParams::mesh_dim_z-1)
	    if (inc_z>0) inc_z=0;

	if (rand()%2)
	    current.x +=inc_x;
	else
	    current.y +=inc_y;
    }
    return coord2Id(current);
}


int roulette()
{
    int slices = GlobalParams::mesh_dim_x + GlobalParams::mesh_dim_y + GlobalParams::mesh_dim_z -2;


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
	max_id = (GlobalParams::mesh_dim_x * GlobalParams::mesh_dim_y * GlobalParams::mesh_dim_z) - 1; //Mesh 
    else    // other delta topologies
	max_id = GlobalParams::n_delta_tiles-1; 

    // Random destination distribution
    do {
	p.dst_id = randInt(0, max_id);

	// check for hotspot destination
	for (size_t i = 0; i < GlobalParams::hotspots.size(); i++) {

	    if (rnd >= range_start && rnd < range_start + GlobalParams::hotspots[i].second) {
		if (local_id != GlobalParams::hotspots[i].first ) {
		    p.dst_id = GlobalParams::hotspots[i].first;
		}
		break;
	    } else
		range_start += GlobalParams::hotspots[i].second;	// try next
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
    src.z = id2Coord(p.src_id).z;
    dst.x = GlobalParams::mesh_dim_x - 1 - src.y;
    dst.y = GlobalParams::mesh_dim_y - 1 - src.x;
    dst.z = GlobalParams::mesh_dim_z - 1 - src.z;
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

    int nbits =
	(int)
	log2ceil((double)
		 (GlobalParams::mesh_dim_x *
		  GlobalParams::mesh_dim_y));
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

    int nbits =
	(int)
	log2ceil((double)
		 (GlobalParams::mesh_dim_x *
		  GlobalParams::mesh_dim_y));
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

Packet ProcessingElement::trafficButterfly()
{

    int nbits = (int) log2ceil((double)
		 (GlobalParams::mesh_dim_x *
		  GlobalParams::mesh_dim_y));
    int dnode = 0;
    for (int i = 1; i < nbits - 1; i++)
	setBit(dnode, i, getBit(local_id, i));
    setBit(dnode, 0, getBit(local_id, nbits - 1));
    setBit(dnode, nbits - 1, getBit(local_id, 0));

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

