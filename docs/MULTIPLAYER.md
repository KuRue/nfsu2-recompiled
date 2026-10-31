# Multiplayer and network modes

Choose the network under **Game → Network**. These choices serve different paths;
connecting to a virtual LAN does not create an EA account or sign into Online mode.

| Selection | Purpose |
| --- | --- |
| nfsu2.xyz | Native free-roam client and a WebSocket virtual LAN, without a browser or shared Wi-Fi requirement |
| Local LAN | The game's LAN discovery on the same network; prefers Wi-Fi |
| EA / Online server | The game's separate Online connection through the saved EA Nation replacement server |

## nfsu2.xyz

Select nfsu2.xyz, start the game and enter career Explore/free roam. The native
client connects to the switch and can show other players' cars, names, nearby
minimap markers and Players markers on the world map. Names/projection account
for the phone's wide aspect ratio.

**Game → Nearby cars** controls the local remote-car rendering budget: any count
from 0–32, default 3. Restart the game process to apply a change. Zero is map-only;
raising the number does not guarantee that many peers are available or loaded.
The server's known players/map markers are not the same as locally rendered cars.

The virtual-LAN transport also carries the game's LAN traffic. The game's Online
mode still uses the saved Online server. Cross-client races/lobbies and every
multiplayer mode have not all been validated; do not infer full compatibility from
a successful switch connection.

Use **Support → Test nfsu2.xyz** for a connection probe. This checks transport
reachability, not whether every game mode can start or stay synchronized.

## Same-network LAN

Use the game's LAN menus with peers on the same Wi-Fi/LAN. Guest networks, client
isolation, VPN routing or blocked broadcasts can prevent discovery even when the
phones have Internet access. Android Wi-Fi multicast handling is enabled while
needed. No public server is used for this local selection.

## Online / EA Nation replacement

Select the EA/Online network option and configure **Online server**. Account creation,
credentials, server availability and game compatibility belong to that external
service. An nfsu2.xyz player identity is not an EA-server account. This project does
not bundle credentials or promise that a third-party server will remain available.

## Connection problems

Record the selected mode, whether the switch probe succeeds, and the exact stage
that fails: discovery, peer appearance, lobby join or race start. For crowd slowdown,
compare the same area with a lower nearby-car budget and cheaper car/reflection
settings. See [Troubleshooting](TROUBLESHOOTING.md) for a useful report checklist.

[Graphics](GRAPHICS.md) · [Controls](CONTROLS.md) · [Index](README.md)
