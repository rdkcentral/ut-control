import asyncio
import websocket

ws = websocket.WebSocket()

async def send_message():
    uri = "ws://localhost:8080"
    async with ws.connect(uri) as websocket:
        while True:
            message = input("Enter message to send to server: ")
            await ws.send(message)
            print(f"Sent: {message}")
	    # Assuming the server echoes the message back
            try:
                response = await ws.recv()
                print(f"Received: {response}")
            except ws.ConnectionClosed:
                print("Connection closed")
                break
asyncio.get_event_loop().run_until_complete(send_message())
