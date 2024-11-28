# cscd58-load-balancer

A simple implementation of a reverse proxy and layer 7 load balancer like nginx or Cloudflare's CDN.
This project is built for the UTSC course CSCD58 - Computer Networks.

## Goal
The goal of this project is to learn more about designing and implementing a reverse proxy.
I want to simulate a load balancer's affects on different network conditions, and test and compare 
benchmarks for packet delays for a client when considering:
- Different load balancer algorithms, liike round-robin, weighted round-robin or least-connections.
- The caching of content for a load balancer when servers may be geographically distant.

Along with that, I want to explore how a reverse proxy can minimize DDoS attacks and examine the
loads on servers with and without one.

### Team Members
Risheit Munshi


## Compiling and Running
This project is written in C++17. 

### Loading the CMake project
Before compilation can occur, the cmake project needs to be loaded.
This only needs to be done once, after cloning the repository, or if any CMakeLists.txt files are modified.

Create a build directory (CMake will not create one itself):
```
mkdir build/
```
Load the cmake project:
```
cmake -B build/
```

### Compilation
Compile the executable after loading the project or modifying and source files.
```
cmake --build build/
```

## Running the Executable

> [!IMPORTANT]
> TODO: Fill this section up

## Features

The following are a planned list of possible features.

- [x] (Weighted) Round-robin load balancing
- [ ] Least-connections load balancing
- [ ] Sticky sessions for load balancing
- [ ] Caching sessions
- [ ] Server health checks
- [ ] Automatic configuration of new servers
- [ ] Anti-DDoS Measures

## Resource Links
- [Network Load Balancing](https://www.techtarget.com/searchdisasterrecovery/definition/Network-Load-Balancing-NLB)
- [Round Robin Load Balancing](https://www.vmware.com/topics/round-robin-load-balancing)
