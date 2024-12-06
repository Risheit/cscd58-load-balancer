# cscd58-load-balancer

A simple implementation of a network load balancer like nginx or Cloudflare's CDN.

This project is built for the UTSC course CSCD58 - Computer Networks.

[Visit the GitHub repository here.](https://github.com/Risheit/cscd58-load-balancer)

## Goal
The goal of this project is to learn more about designing and implementing a reverse proxy.
I want to simulate a load balancer's affects on different network conditions, and test and compare 
benchmarks for packet delays for a client when considering:
- Different load balancer algorithms, liike round-robin, weighted round-robin or least-connections.
- What happens when underlying servers fail, and fallbacks and alternatives in those cases. 

### Team Members
Risheit Munshi


## Compiling and Running
This project is written using CMake and C++17. and built to run on the [mininet 2.3.0 VM image](https://github.com/mininet/mininet/releases/tag/2.3.0). Mininet setup scripts and example web servers are written using Python.

CMake is not installed by default on the Mininet VM releases. Please install it manually through:
```
sudo apt update
sudo apt install cmake 
```

After installing, and moving to the `cscd58-load-balancer/` directory, make sure to run `git pull origin/main` to fetch the latest version of the repository.

### Loading the CMake project
Before compilation, the CMake project needs to be loaded. Before beginning, make sure to `cd` to whichever directory you have cloned this repository to.

This step needs to be performed:
- During the initial setup process, after cloning the repository
- Whenever any `CMakeLists.txt` files are modified

The following command loads the CMake project, creating the necessary Makefiles to compile and writing to the `./build/` directory.
```
cmake -S. -B./build -G "Unix Makefiles"
```

### Compilation
Compile the executable into the `./build/` directory after the project has been loaded or if any source files have been modified.
```
cmake --build build/
```

### Running the Executable

The executable is run on the command line, and written into the `./build/bin/` directory. For quick reference on the flags and options you can pass in, pass in the `-h` or `--help` flag.

```
./LoadBalancer [-p | --port {port}] [-t | --stale {seconds}] [-r | --retries {amt}] [-c | --connections {amt}] [--log LEVEL] [strategy] { ip_addr1   port1   weight1 } ...
```

The file takes in servers through command line arguments. For each server you want to the load balancer to listen to, provide:
- An IP address of a valid format (for example, `10.0.0.1`)
- A valid port number for the service (for example, `80`)
- The weight of the server. This is used in the weighted round-robin load balancing strategy. This is a positive number corresponding to how many requests will be sent to that server before routing to a different one. This should be a positive number (although a weight of 0 means that the server will never be routed to).

For example, the following command would start the balancer redirecting requests to the servers:
- at `10.0.0.1` on port 80, with a weight of 1.
- at `10.0.0.2` on port 443, with a weight of 4.

```
./LoadBalancer 10.0.0.1 80 1 10.0.0.2 443 4
```

The following flags exist on the load balancer. Flags need to be placed before the positional arguments to be valid.
- `-p`, `--port` sets the port the load balancer starts on. The default port is `40192`.
- `-t`, `--stale` sets the time period of inactivity that a server will go through before the load balancer sends an is alive request. By default, this is `30` seconds.
- `-r`, `--retries` sets the amount of times a request to an underlying server that fails is retried on a different server. After this retry amount, an HTTP 503 error is sent back to the client. By default, this is retry amount is `3`.
- `-c`, `--connections` sets the size of the connections backlog the local socket the balancer can handle. In the underlying code, it calls `listen(..., connections)` when starting the balancer server. By default, this is 5.
- `--log` is a number that sets the log level of the balancer. The higher the log level, the more is shown, going from errors > warnings > info > verbose > debug. By default this is `3` (showing errors, warnings, and info logs).
- `--robin`, `--least`, `--random` set the strategy being used for load balancing. Only one of these can be present when calling the balancer. By default, this is `--robin`.
  - `--robin` starts the load balancer using a weighted round robin algorithm
  - `--least` starts the load balancer using a least connections algorithm
  - `--random` starts the load balancer randomly selecting connected servers

### Running the Mininet examples
The following mininet commands, located under the `./mininet/` directory, are Python scripts. If you run into an issue running them directly, please make sure you have the necessary executable permissions on the file or call them through the python interpreter:
```
python [filename]
```

### Basic Webserver (`server.py`)
```
usage: server.py [-h] [-l LISTEN] [-p PORT] [-d DELAY] name

Run a simple HTTP server

positional arguments:
  name                  The name of this web server

optional arguments:
  -h, --help            show this help message and exit
  -l LISTEN, --listen LISTEN
                        Specify the IP address on which the server listens
  -p PORT, --port PORT  Specify the port on which the server listens
  -d DELAY, --delay DELAY
                        Specify in seconds on how long the server takes to respond
```

Based off the server by [github user bradmontgomery](https://gist.github.com/bradmontgomery/2219997). It provides the option to pass in a delay in seconds, to simulate long running server processes or slow net connections.
The server can handle GET, HEAD, and POST requests made to it, and responds with dummy html.

By default, the server opens at 0.0.0.0 on port 8000.

### Random Webserver (`random_server.py`)
```
usage: random_server.py [-h] [-l LISTEN] [-p PORT] [-M MAX] [-m MIN] name

Run a simple HTTP server with a possible random delay from (min - max).

positional arguments:
  name                  The name of this web server

optional arguments:
  -h, --help            show this help message and exit
  -l LISTEN, --listen LISTEN
                        Specify the IP address on which the server listens
  -p PORT, --port PORT  Specify the port on which the server listens
  -M MAX, --max MAX     Specify in seconds the maximum time the server can take to respond.
  -m MIN, --min MIN     Specify in seconds the minimum time the server can take to respond.
```

Identical to our standard webserver with one key difference. The random webserver takes in a minimum and maximum delay, and responds with a delay in [`min`, `max`] chosen randomly and uniformly.

### Basic Load Balancer (`basic_loadbalancer.py`)
```
usage: basic_loadbalancer.py [-h] [-c CLIENTS] [-s SERVERS] [-d DELAY] [-r STRATEGY] [-t STALE] [--log LOG]

Boot up Mininet with a topology with clients, servers, and a load balancer all connected up to a single switch.

optional arguments:
  -h, --help            show this help message and exit
  -c CLIENTS, --clients CLIENTS
                        Specify the number of clients in this topo.
  -s SERVERS, --servers SERVERS
                        Specify the server on which the server listens.
  -d DELAY, --delay DELAY
                        Specify in seconds on how long the each server takes to respond.The argument for this option is passed in as
                        [-s DELAY] to the servers.
  -r STRATEGY, --strategy STRATEGY
                        The strategy to use. The argument for this option is passed in as [--strategy] to the load balancer.
  -t STALE, --stale STALE
                        Timeout before the balancer checks if a server is active.
  --log LOG             The logging level of the balancer.
```

This begins a Mininet network that starts up a set of `c` clients, `h` servers running the `./mininet/server.py` server, and connects them all to a single switch. All these servers have the same weight of 1, and are named after their host name on Mininet: `hX`, where `X` is some number. 

While here, run `link s1 hX down/up` to test the load balancer under different conditions.
Query the load balancer from some client `hY` through `hY curl h1` or `hY curl 10.10.10.10`. 

### Tests (`test_[name].py`)
These are different mininet configuration styles to test how different strategies function under different load conditions.

```
optional arguments:
  -h, --help            show this help message and exit
  -t STRATEGY, --strategy STRATEGY
                        The strategy to use. The argument for this option is passed in as [--strategy] to the load balancer.
```

Provide a `strategy` and simulates congestion by running `curl` commands made to the balancer. 
The file provides clients, hosts, weights. Each client makes 3 `curl` requests in parallel, and each client runs in parrallel.
In finishes by printing the final out, containing the total, mean, and median times taken for each client.

The available tests are:
- `test_diff_delay_diff_weights`: Test a series of backing servers, with some servers 

## Further Documentation
See more details on implementation and exploration under the `docs/` directory.

## Features

The following are a planned list of possible features.

- [x] (Weighted) Round-robin load balancing
- [x] Least-connections load balancing
- [x] Server health checks
- [x] Automatic fallback handling for when servers go down.
- [ ] Sticky sessions for load balancing
- [ ] Caching sessions
- [ ] Automatic configuration of new servers
- [ ] Anti-DDoS Measures

## Resource Links
- [Network Load Balancing](https://www.techtarget.com/searchdisasterrecovery/definition/Network-Load-Balancing-NLB)
- [Round Robin Load Balancing](https://www.vmware.com/topics/round-robin-load-balancing)
