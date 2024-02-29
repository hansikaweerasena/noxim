/*
 * Hansika Weerasena - 2024/02/29
 */

#ifndef __NEWGLOBALTRACEINJECTOR_H__
#define __NEWGLOBALTRACEINJECTOR_H__

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <queue>
#include "DataStructs.h"

using namespace std;

// Structures used to store information into the trace injector
struct Message {
    int src;
    int dest;
    string size;
    int addr;
    string type;
    bool valid; // Flag to indicate if the message is valid
};

struct Record {
    Message out_msg;
    Message in_msg;
    int delay;
};


class GlobalTraceInjector {

  public:

    GlobalTraceInjector();

    // Load traffic table from file. Returns true if ok, false otherwise
    bool load(const int no_of_traces);

    // Returns the record with next in message to network depend on the out message
    Record getNextRecord(const int trace_id,
			       const int src,
			       const int des,
                   const int addr);

  private:

    Message parseMessage(const std::string& msgPart)

    std::queue<Record> readData(const string& filename)

    vector<std::queue<Record>> traces;
};

#endif
