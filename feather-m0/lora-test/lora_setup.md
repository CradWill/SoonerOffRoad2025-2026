LoRa Test Setup Instructions 

1. Setup 

Download both folders: 

Test_Node_TX 

Test_Base_RX 

Make sure TestProtocol.h is inside BOTH folders 

2. Upload Code 

Upload Test_Node_TX.ino to one Feather 

Upload Test_Base_RX.ino to the other Feather 

3. Hardware Roles 

Node (battery-powered): 

Runs Test_Node_TX 

This is the remote device 

Base (connected to PC via USB): 

Runs Test_Base_RX 

This is your control + monitoring device 

4. Running the Test 

Power both Feathers 

Open Serial Monitor (115200 baud) on the Base (PC-connected Feather) 

You should see incoming messages from the Node 

In the Serial Monitor, type: 

ping 

The Node should respond with: 

PONG 

5. Expected Behavior 

Node sends messages automatically every ~2 seconds 

Base receives and prints messages + RSSI 

Base can send commands (ping) and receive responses 

6. Notes 

Both radios must use the same frequency (default: 915 MHz) 

Keep devices a few feet apart for initial testing 
