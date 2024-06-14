#!/usr/bin/env python3
# /*
#  * If not stated otherwise in this file or this component's LICENSE file the
#  * following copyright and licenses apply:
#  *
#  * Copyright 2023 RDK Management
#  *
#  * Licensed under the Apache License, Version 2.0 (the "License");
#  * you may not use this file except in compliance with the License.
#  * You may obtain a copy of the License at
#  *
#  * http://www.apache.org/licenses/LICENSE-2.0
#  *
#  * Unless required by applicable law or agreed to in writing, software
#  * distributed under the License is distributed on an "AS IS" BASIS,
#  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  * See the License for the specific language governing permissions and
#  * limitations under the License.
#  */
#  usage : <filename.py>

import os
import asyncio
import websockets
import json

# Function to read YAML file
def read_json(file_path):
    with open(file_path, 'r') as file:
        return json.load(file)

# Async function to send YAML via WebSocket
async def send_yaml_via_websocket(uri, json_data):
    async with websockets.connect(uri) as websocket:
        await websocket.send(json.dumps(json_data))
        print("JSON data sent")

# Example usage
if __name__ == "__main__":
    file_name = "example.json"
    absolute_path = os.path.abspath(file_name)
    json_file_path = absolute_path;
    websocket_uri = 'ws://localhost:8080'

    json_data = read_json(json_file_path)
    asyncio.get_event_loop().run_until_complete(send_yaml_via_websocket(websocket_uri, json_data))

