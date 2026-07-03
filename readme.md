Roooter: OSPF Self-Healing Network Simulator 🚀

Roooter is a high-performance, fault-tolerant network routing daemon inspired by enterprise Cisco SD-WAN architecture. It transforms a standard graph-theory pathfinder into a dynamic, event-driven simulation engine.

This project showcases advanced systems engineering patterns, including multi-threaded C++ execution, raw byte-level Inter-Process Communication (IPC), and real-time telemetry extraction.


(Include a screenshot of your dashboard here)

🧠 Core Architecture & Tech Stack

This project is built across three distinct architectural layers:

Algorithmic Core (C++): A multi-threaded daemon running an optimized Dijkstra's Algorithm (O((V+E)log V)). It features a "Chaos Simulator" thread that randomly injects link failures (cost = infinity) while the main thread safely recalculates optimal routes using Mutex locks (std::mutex).

Systems-Level IPC (Binary Serialization): Instead of slow string parsing, C++ communicates with Python via a strict, byte-level binary protocol over standard I/O pipes. It utilizes a 0xAA55AA55 hex sync-word and IEEE 802.3 CRC32 mathematical hashing to guarantee memory alignment and prevent bit-flipping corruption.

Telemetry Agent & UI (Python/FastAPI + Vanilla JS): A lightweight Python middleware acts as an observability agent, unpacking the binary stream, deriving real-time metrics (Throughput, Convergence Time, Drop Rate), and blasting it to a zero-dependency Vanilla JS frontend over WebSockets at 60 FPS.

Technologies Used

Backend Engine: Modern C++ (C++11/14), <thread>, <mutex>, <cstdint>

Middleware/API: Python 3.8+, FastAPI, Uvicorn, subprocess, struct (Binary Unpacking)

Frontend: HTML5, Vanilla JavaScript, Tailwind CSS (via CDN), Vis-Network (Physics Engine), Chart.js

⚙️ Key Features

Dynamic Self-Healing: The background thread artificially sabotages cables; the engine detects this and reroutes traffic automatically in milliseconds.

Sliding Window Sync Hunter: Python uses a 1-byte sliding window to hunt for the packet header, ensuring crash-proof telemetry ingestion.

Barnes-Hut Physics Visualization: Nodes untangle themselves dynamically using a physics engine, locking automatically to preserve 0% CPU footprint while waiting for routing updates.

Continuous "Heartbeat" Charting: Smooth-scrolling Chart.js integration mimics AWS CloudWatch/Datadog interfaces.

🚀 How to Run Locally

Prerequisites

GCC Compiler (for compiling C++ with the -O3 optimization flag)

Python 3.8+

Python Packages: fastapi, uvicorn, websockets

Installation & Execution

Clone the repository:

git clone [https://github.com/yourusername/roooter-network-sim.git](https://github.com/yourusername/roooter-network-sim.git)
cd roooter-network-sim


Compile the C++ Engine:
Run the following command to compile the C++ code into an executable with Level 3 extreme hardware optimizations:

g++ -O3 networkdefining_phase3.cpp -o a.exe


(Note: For Linux/Mac, omit .exe and update api_dashboard.py line 129 to point to ./a.out)

Start the Python Telemetry Server:
Boot up the FastAPI server which will automatically manage the C++ daemon:

python -m uvicorn api_dashboard:app --reload


Launch the Dashboard:
Simply double-click index.html to open it in your browser (Chrome/Edge/Firefox). No local web server is strictly required for the HTML file, but you can also host it via Live Server.

Simulate!
Click Deploy on the dashboard. Watch the C++ engine randomly break links and heal the network while the telemetry graphs update in real-time.

🐛 Known Windows Security Workarounds

If you encounter OSError: [WinError 4551], Windows Defender/AppLocker is blocking the newly compiled a.exe. To fix:

Right-click a.exe in File Explorer -> Properties.

Check Unblock at the bottom of the General tab and click Apply.