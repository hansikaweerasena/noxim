#pragma once

#include <queue>
#include <vector>
#include <deque>
#include <algorithm>
#include "DataStructs.h"

class TSV {
    private:
        int x, y;  // Coordinates of the TSV
        std::deque<int> routerReqs; //Routers in order of being served (round-robin)

    public:
        TSV(int x, int y);
        bool reqAccess(int router_id);  // Local arbitration for access
        std::queue<Flit> tsv_buffer;  // Flit on bus currently
        int direction = -1; //Current flit direction
    };