/*
 * Hansika Weerasena - 2024/02/29
 */

#include "GlobalTraceInjector.h"

#include <sstream>
#include <stdexcept>

GlobalTraceInjector::GlobalTraceInjector()
{
}

// Load traffic table from file. Returns true if ok, false otherwise
bool GlobalTraceInjector::load(const int no_of_traces)
{
    vector<string> filenames;

    // Generate filenames based on the number of files
    for (int i = 0; i < no_of_traces; ++i) {
        stringstream ss;
        ss << "data" << i << ".txt";
        filenames.push_back(ss.str());
    }

    // Load each file into a separate queue
    for (const auto& filename : filenames) {
        try {
            auto recordsQueue = readData(filename);
            traces.push_back(std::move(recordsQueue));
        } catch (const std::runtime_error& e) {
            cerr << "Error processing " << filename << ": " << e.what() << endl;
            return false;
        }
    }

  return true;
}


// Parse a message from a string
Message GlobalTraceInjector::parseMessage(const std::string& msgPart) {
    Message msg;
    msg.valid = false; // Default to false

    if (msgPart == "None") {
        return msg; // Return invalid message if "None"
    }

    size_t startPos = msgPart.find('[');
    size_t endPos = msgPart.find(']');
    if (startPos != string::npos && endPos != string::npos) {
        string content = msgPart.substr(startPos + 1, endPos - startPos - 1);
        istringstream contentStream(content);
        string token;

        while (getline(contentStream, token, ',')) {
            istringstream tokenStream(token);
            string key, value;

            getline(getline(tokenStream, key, '='), value);
            key.erase(remove_if(key.begin(), key.end(), ::isspace), key.end());

            if (key == "src") msg.src = stoi(value);
            else if (key == "dest") msg.dest = stoi(value);
            else if (key == "size") msg.size = stoi(value);
            else if (key == "addr") msg.addr = stoi(value);
            else if (key == "type") msg.type = value;
        }

        msg.valid = true;
    }

    return msg;
}


std::queue<Record> GlobalTraceInjector::readData(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        throw runtime_error("File not found: " + filename);
    }

    std::queue<Record> records;
    string line;

    while (getline(file, line)) {
        stringstream ss(line);
        Record record;
        string outMsgPart, inMsgPart, delayPart;

        getline(ss, outMsgPart, ':');
        getline(ss, inMsgPart, ':');
        getline(ss, delayPart);

        record.out_msg = parseMessage(outMsgPart);
        record.in_msg = parseMessage(inMsgPart);
        record.delay = stoi(delayPart.substr(delayPart.find("=") + 1));

        records.push(record);
    }

    return records;
}


// Returns the next record for a given trace id
Record GlobalTraceInjector::getNextRecord(const int trace_id,
                                          const int src,
                                          const int dest,
                                          const int addr)
{
    if (trace_id >= 0 && trace_id < traces.size()) {
        std::queue<Record>& targetQueue = traces[trace_id];

        if (!targetQueue.empty()) {
            const Record& frontRecord = targetQueue.front();

            if (frontRecord.out_msg.src == src && frontRecord.out_msg.dest == dest && frontRecord.out_msg.addr == addr) {
                Record returnRecord = frontRecord;
                targetQueue.pop();
                return returnRecord;
            } else {
                throw std::runtime_error("No matching out message found.");
            }
        } else {
            throw std::runtime_error("Queue is empty.");
        }
    } else {
        std::cerr << "Index out of range. Invalid Trace ID" << std::endl;
    }

}


/*
// Returns the first in messages and trace id for a given src
*/
std::vector<FirstInMsg> GlobalTraceInjector::getFirstInMsgs(const int src)
{
    std::vector<FirstInMsg> firstInMsgs;
    for (auto& queue : traces) {
        if (!queue.empty()) {
            const Record& frontRecord = queue.front();

            if (frontRecord.in_msg.src == src && !frontRecord.out_msg.valid) {
                FirstInMsg firstInMsg;
                firstInMsg.in_msg = frontRecord.in_msg;
                firstInMsg.trace_id = &queue - &traces[0]; // Get the index of the queue in the traces vector
                firstInMsgs.push_back(firstInMsg);
                queue.pop();
            }
        }
    }

    return firstInMsgs;

}