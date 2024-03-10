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
    int size;
    int addr;
    string type;
    bool valid; // Flag to indicate if the message is valid
};

struct Record {
    Message out_msg;
    Message in_msg;
    int delay;
};

struct FirstInMsg {
    Message in_msg;
    int trace_id;
};

class GlobalTraceInjector {

  public:

    GlobalTraceInjector();

    // Load traffic table from file. Returns true if ok, false otherwise
    bool load(const int no_of_traces, const int node_offset);

    // Returns the record with next in message to network depend on the out message
    Record getNextRecord(const int trace_id,
			       const int src,
			       const int dest,
                   const int addr);

    // Returns the first in messages and trace id for a given src
    std::vector<FirstInMsg> getFirstInMsgs(const int src);

  private:

    Message parseMessage(const std::string& msgPart, const int node_offset);

    std::queue<Record> readData(const string& filename, const int node_offset);

    std::vector<std::queue<Record>> traces;
};

#endif
