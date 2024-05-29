import asyncio
import websockets
import yaml

async def send_message():
    uri = "ws://localhost:8080"
    async with websockets.connect(uri) as websocket:
        while True:
            key = input("Enter key: ")
            value = input("Enter value: ")
            
            # Create a dictionary and serialize it to a YAML string
            data = {"key": key, "value": value}
            message = yaml.dump(data)
            await websocket.send(message)
            print(f"Sent: {message}")
            
            # Receive and deserialize YAML response
            try:
                response = await websocket.recv()
                response_data = yaml.safe_load(response)
                print(f"Received: {response_data}")
            except websockets.ConnectionClosed:
                print("Connection closed")
                break

asyncio.get_event_loop().run_until_complete(send_message())
