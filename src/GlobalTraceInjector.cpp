/*
 * Hansika Weerasena - 2024/02/29
 */

#include "GlobalTraceInjector.h"

#include <sstream>
#include <stdexcept>

GlobalTraceInjector::GlobalTraceInjector()
{
}

bool GlobalTraceInjector::load(const int no_of_traces)
{
    vector<string> filenames;

    // Generate filenames based on the number of files
    for (int i = 1; i <= no_of_traces; ++i) {
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
            return false
        }
    }

  return true;
}



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
            else if (key == "size") msg.size = value;
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


// TODO: use this kind of exception handling when using this
//try {
//// Code that may throw a runtime_error
//} catch (const std::runtime_error& e) {
//std::string errorMessage = e.what();
//if (errorMessage == "Queue is empty.") {
//// Handle empty queue specifically
//} else {
//// Handle other runtime errors
//}
//}

Record GlobalTraceInjector::getNextRecord(const int trace_id,
                                          const int src,
                                          const int des,
                                          const int addr)
{
    if (index >= 0 && index < traces.size()) {
        std::queue<Record>& targetQueue = traces[index];

        if (!targetQueue.empty()) {
            const Record& frontRecord = specificQueue.front();

            if (frontRecord.out_msg.src == src && frontRecord.out_msg.src == src)


            std::cout << "Front record of queue " << index + 1 << " out_msg src: " << frontRecord.out_msg.src << std::endl;

            // If you need to remove the front record
            // specificQueue.pop();
        } else {
            throw std::runtime_error("Queue is empty.");
        }
    } else {
        std::cerr << "Index out of range. Invalid Trace ID" << std::endl;
    }

}
