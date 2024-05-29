import asyncio
import websockets

async def send_message():
    uri = "ws://localhost:8080"
    async with websockets.connect(uri) as websocket:
        while True:
            message = input("Enter message to send to server: ")
            await websocket.send(message)
            print(f"Sent: {message}")
            # Assuming the server echoes the message back
            try:
                response = await websocket.recv()
                print(f"Received: {response}")
            except websockets.ConnectionClosed:
                print("Connection closed")
                break

asyncio.get_event_loop().run_until_complete(send_message())
