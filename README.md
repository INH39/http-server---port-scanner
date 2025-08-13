# HTTP Server and Port Scanner

A multithreaded HTTP server and port scanner, did this project when I first started learning C to learn more about fundamental networking concepts and socket programming while learning C.

## Features

- **Multithreaded HTTP Server** - Handles concurrent GET/POST requests with file serving capabilities
- **Security System** - Detects and blacklists IPs based on excessive connection attempts (rate limiting)
- **Port Scanner** - Identifies open ports on target servers
- **Request Logging** - Monitors and logs HTTP requests and connection attempts
- **Attack Simulation** - Tests blacklist functionality with simulated connection floods

## How to Run

### Compile
```bash
make
```

### Start HTTP Server
```bash
./server
```
Server will listen on `http://localhost:8080`

### Test GET Requests (while server runs)
Visit `http://localhost:8080/test_get.html` in your browser or:
```bash
curl http://localhost:8080/test_get.html --output response.html
```
Then open `response.html` to see the server's response.

### Test POST Requests (while server runs)
```bash
python3 test_post.py
```

### Test Port Scanner (while server runs)
```bash
./port_scanner
```

### Test Blacklist System (while server runs)
```bash
./blacklist_test
```

