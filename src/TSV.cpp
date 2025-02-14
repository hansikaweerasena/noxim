#include "TSV.h"

TSV::TSV(int x, int y) {
    this->x = x;
    this->y = y;
}
        
bool TSV::reqAccess(int router_id) {
    if (!tsv_buffer.empty()) {
        //TSV has already been loaded for this cycle
        return false;
    }
    else {
        //Check if the router is already waiting
        auto it = std::find(routerReqs.begin(), routerReqs.end(), router_id);
        if (it == routerReqs.end()) {
            //If not waiting, add to end of queue
            routerReqs.push_back(router_id);
        }
        //Only give access if at front of queue
        if (routerReqs.front() == router_id) {
            //Remove from queue and grant access
            routerReqs.pop_front();
            return true;
        }
        else {
            //Not its turn yet
            return false;
        }
    }
}