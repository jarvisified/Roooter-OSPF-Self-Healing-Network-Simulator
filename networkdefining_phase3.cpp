#include <iostream>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <random>
#include <climits>
#include <algorithm>
#include <cstdint>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

using namespace std;

const int LS_INFINITY = 1000000;

// CRC32 checksum calculation function
// This mathematically hashes the payload. If a single bit flips in memory
// during the transfer to Python, the hash changes, and Python will reject it

uint32_t calculateCRC32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

// creating a network link
struct Edge
{
    int destination;
    int cost;
};

// structured way to return metric cost and actual path taken
struct pathInfo
{
    int totalCost;
    vector<int> path;
};

// this acts as our message queue, simulating the network packet transmission between routers
mutex queueMutex;
condition_variable networkEventTrigger;
bool networkChanged = true; // start with true to calculate first route

class Network
{
private:
    vector<vector<Edge>> adjList;
    vector<pair<int, int>> activeEdges; // store the active edges for random selection
    mutex graphMutex;                   // prevents the simulation thread from changing the graph while we are calculating the shortest path
public:
    Network(int nodes) : adjList(nodes) {}

    void addLink(int routerA, int routerB, int cost)
    {
        lock_guard<mutex> lock(graphMutex);
        adjList[routerA].push_back({routerB, cost});
        adjList[routerB].push_back({routerA, cost});
        activeEdges.push_back({routerA, routerB});
    }

    pair<int, int> getRandomEdge(mt19937 &rng)
    {
        lock_guard<mutex> lock(graphMutex);
        if (activeEdges.empty())
        {
            return {-1, -1}; // no edges to select
        }
        uniform_int_distribution<int> dist(0, activeEdges.size() - 1);
        return activeEdges[dist(rng)];
    }

    // self healing mechanism
    int updateEdge(int u, int v, int newCost)
    {
        lock_guard<mutex> lock(graphMutex);
        int oldCost = -1;
        for (auto &edge : adjList[u])
        {
            if (edge.destination == v)
            {
                oldCost = edge.cost;
                edge.cost = newCost;
            }
        }
        for (auto &edge : adjList[v])
        {
            if (edge.destination == u)
            {
                edge.cost = newCost;
            }
        }
        return oldCost;
    }

    pathInfo getShortestPath(int source, int destination)
    {
        // lock the graph so simulation thread doesn't change the graph while we are calculating the shortest path
        lock_guard<mutex> lock(graphMutex);

        pathInfo result;
        result.totalCost = -1;

        int n = adjList.size();
        vector<int> dist(n, INT_MAX);
        vector<int> parent(n, -1);
        priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;

        dist[source] = 0;
        pq.push({0, source});

        while (!pq.empty())
        {
            int currentCost = pq.top().first;
            int currentRouter = pq.top().second;
            pq.pop();

            if (currentCost > dist[currentRouter])
            {
                continue; // skip if we have already found a cheaper path to this router
            }
            if (currentRouter == destination)
            {
                break; // stop if we reached the destination
            }

            // Explore all connected cables from the current router
            for (const Edge &edge : adjList[currentRouter])
            {
                int nextRouter = edge.destination;
                int newCost = edge.cost;

                // Relaxation step: if the new cost is cheaper, update the minCost and parentMap
                if (dist[currentRouter] + newCost < dist[nextRouter])
                {
                    dist[nextRouter] = dist[currentRouter] + newCost;
                    parent[nextRouter] = currentRouter;
                    pq.push({dist[nextRouter], nextRouter});
                }
            }
        }

        // backtrack to find the path
        if (dist[destination] != INT_MAX)
        {
            result.totalCost = dist[destination];
            for (int at = destination; at != -1; at = parent[at])
            {
                result.path.push_back(at);
            }
            reverse(result.path.begin(), result.path.end());
        }

        return result;
    }
};

// function to simulate the network behavior
void networkSimulator(Network &net)
{
    mt19937 rng(time(0));
    uniform_int_distribution<int> timehealthy(100, 500);   // 0.1-0.5 sec of normal operation
    uniform_int_distribution<int> timebroken(5000, 10000); // 5-10 sec of CHAOS

    while (true)
    {
        this_thread::sleep_for(chrono::milliseconds(timehealthy(rng)));

        // pick a random edge from network the user defined and change its cost
        pair<int, int> edgeToBreak = net.getRandomEdge(rng);
        if (edgeToBreak.first == -1)
        {
            continue; // no edges to break
        }

        int u = edgeToBreak.first;
        int v = edgeToBreak.second;

        int originalCost = net.updateEdge(u, v, LS_INFINITY); // simulate a broken link by setting cost to infinity
        {
            lock_guard<mutex> lock(queueMutex);
            networkChanged = true;
        }
        networkEventTrigger.notify_one(); // notify the main thread that the network has changed

        this_thread::sleep_for(chrono::milliseconds(timebroken(rng))); // wait for some time before restoring the link

        net.updateEdge(u, v, originalCost); // restore the original cost
        {
            lock_guard<mutex> lock(queueMutex);
            networkChanged = true;
        }
        networkEventTrigger.notify_one();
    }
}

int main()
{
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stdin), _O_BINARY);
#endif

    int numNodes, numEdges, source, destination;
    cin >> numNodes >> numEdges >> source >> destination;

    Network net(numNodes);

    for (int i = 0; i < numEdges; i++)
    {
        int u, v, cost;
        cin >> u >> v >> cost;
        net.addLink(u, v, cost);
    }

    // start the network simulation in a separate thread
    thread simulatorThread(networkSimulator, ref(net));

    const uint32_t SYNC_WORD = 0xAA55AA55; // The universal Header Sync Word (Helps Python know exactly where a packet begins)

    while (true)
    {
        unique_lock<mutex> lock(queueMutex);
        networkEventTrigger.wait(lock, []
                                 { return networkChanged; }); // wait for a network change event
        networkChanged = false;                               // reset the flag
        lock.unlock();

        pathInfo initialRoute = net.getShortestPath(source, destination);

        vector<uint32_t> packet; // buffer to hold the packet

        // path not found
        if (initialRoute.totalCost == -1)
        {
            // failure status check
            packet.push_back(static_cast<uint32_t>(-1)); // add the sync word to the payload
            packet.push_back(0);                         // add the path length as 0 since no path exists
        }
        else
        {
            // success state
            packet.push_back(static_cast<uint32_t>(initialRoute.totalCost));
            packet.push_back(static_cast<uint32_t>(initialRoute.path.size()));

            for (int router : initialRoute.path)
            {
                packet.push_back(static_cast<uint32_t>(router)); // load the actual router IDs into the packet
            }
        }

        size_t totalBytes = packet.size() * sizeof(uint32_t);                                             // calculate the total size of the packet in bytes
        uint32_t checkSum = calculateCRC32(reinterpret_cast<const uint8_t *>(packet.data()), totalBytes); // calculate the CRC32 checksum

        // Fire the Structured Packet over the IPC Socket, Format: [SYNC_WORD] -> [CRC32] -> [PAYLOAD]
        cout.write(reinterpret_cast<const char *>(&SYNC_WORD), sizeof(SYNC_WORD));
        cout.write(reinterpret_cast<const char *>(&checkSum), sizeof(checkSum));
        cout.write(reinterpret_cast<const char *>(packet.data()), totalBytes); // send the packet to the output stream
        cout.flush();                                                          // ensure the output is sent immediately
    }

    simulatorThread.join(); // wait for the simulator thread to finish
    return 0;
}