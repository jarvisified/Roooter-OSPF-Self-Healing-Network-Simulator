from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
import asyncio
import json
import threading
import time
import subprocess
import struct
import zlib

app = FastAPI(title="Advanced OSPF Routing Dashboard API")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

def listentodaemon(process: subprocess.Popen, loop: asyncio.AbstractEventLoop, websocket: WebSocket):
    print("Telemetry agent active...")
    last_path = None
    last_time = time.time()

    try:
        while True:
            # Advanced data serialization reading
            # the sliding window sync hunter (we read 1 byte at a time)
            sync_buffer = b""
            while True:
                byte = process.stdout.read(1)
                if not byte:
                    return
            
                sync_buffer += byte
                if len(sync_buffer)>4:
                    sync_buffer = sync_buffer[1:] # keep only last 4 digits
            
                if len(sync_buffer) == 4:
                    sync_word = struct.unpack('<I', sync_buffer)[0]
                    if sync_word == 0xAA55AA55:
                        break
            
            # Read the checksum
            crc_data = process.stdout.read(4)
            received_crc = struct.unpack('<I', crc_data)[0]

            # Read the Payload Header (Cost and hop_count -> 8 bytes)
            payload_header = process.stdout.read(8)
            cost, hop_count = struct.unpack('<II', payload_header)

            # Read the Dynamic Path Array
            path_data = b""
            if hop_count > 0:
                path_data = process.stdout.read(hop_count * 4)
            
            # Security Verification
            full_payload = payload_header + path_data
            computed_crc = zlib.crc32(full_payload) & 0xFFFFFFFF

            if computed_crc != received_crc:
                print(f"CRC Mismatch! Dropping corrupted packet. Expected {received_crc}, Got {computed_crc}")
                continue

            # Unpacking path
            path = list(struct.unpack('<' + ('I'*hop_count), path_data)) if hop_count > 0 else[]

            # Print success to terminal
            print(f"Success! Route parsed. Cost: {-1 if cost == 4294967295 else cost} | Hops: {hop_count}")

            # Telemetry agent logic
            currenttime = time.time()
            broken = (cost == 4294967295)

            # calculates the convergence time (spikes when routes changes)
            convergencems = 0
            if path != last_path and last_path is not None:
                convergencems = round((currenttime - last_time)*1000,2) 
            
            # Deriving throughput (inversely proportional to the cost and maximum value of 1000 mbps)
            throughput = 0 if broken else round((1000 / cost) * 10, 2)

            # Derive packet drop rate
            droprate = 100 if broken else 0

            last_path = path
            last_time = currenttime

            # Return the results as JSON
            data = {
                "type": "telemetry_update",
                "total_cost": -1 if broken else cost, # Handle uint32 max failure code
                "path": path,
                "metrics": {
                    "throughput_mbps": throughput,
                    "convergence_ms": convergencems if convergencems > 0 else 1.2, # Baseline ping
                    "drop_rate_percentage": droprate
                }
            }

            asyncio.run_coroutine_threadsafe(websocket.send_json(data), loop)
    
    except Exception as e:
        print(f"Error:{repr(e)}")
    finally:
        if process.poll() is None:
            process.terminate()

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    process = None
    try:
        loop = asyncio.get_running_loop()
        while True:
            # wait for the browser to send the network layout
            message = await websocket.receive_text()
            data = json.loads(message)
            
            #Format the JSON into raw text input for the C++ binary
            network_input = f"{data['num_nodes']} {data['num_edges']} {data['source']} {data['destination']}\n"
            for edge in data['edges']:
                network_input += f"{edge['u']} {edge['v']} {edge['cost']}\n"
            
            if process and process.poll() is None:
                process.terminate()
            
            # Start C++ engine
            process = subprocess.Popen(
                ['.\\a.exe'],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )

            #Feed the text input into C++
            process.stdin.write(network_input.encode('utf-8'))
            process.stdin.flush()

            # start background telemetry thread
            threading.Thread(target=listentodaemon, args=(process, loop, websocket), daemon=True).start()
    
    except WebSocketDisconnect:
        if process and process.poll() is None:
            process.terminate()