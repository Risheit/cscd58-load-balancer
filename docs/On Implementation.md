# On Implementation

This document contains details about the actual implementation of the load balancer.
Notable files and functions have entries here, explaining the code and the reasons behind them.

Smaller utility classes might not have a description here, as they don't relate very much to the topic at hand. If wanted, more information is likely located within a comment at the beginning of the object's header file.

## `LoadBalancer`

The main class running the load balancer application.
The `LoadBalancer::start(...)` method is the main entry point of this program, starting up a server that takes in socket data from received connections, and then forwards them to set up backing servers.

The balancer uses several helper structs to wrap the data it handles.

### `Connection`

A wrapper for `TcpClient` to add metadata (provided through the `Metadata` structure). 

A *connection* is the term used within the codebase for the relationship between the balancer and its underlying servers, a general reference for both the backing server and the connecting link between that server and the balancer. 
Connections may *go down* or be considered *inactive* when the balancer stops receiving data from the server for any reason, and are considered *stale* if a period of time (specified when constructing the balancer) passes without any requests made to it. More on this when discussing the `LoadBalancer::testServers` method.

### `Transaction`

A *transaction* is the term used within the codebase for the request and response pair made by the balancer to one of its connections. They are created when the balancer makes the inital request, whatever it may be, and  hold a `future`, an asynchronous promise that resolves to a `TransactionResult` after either:
- The backing server responds to the request
- The socket the data is made on times out or fails to connect for some reason

Transaction results contain the response received from the server if it exists, along with information about that data, like the connection it belongs to and which socket that the response should be forwarded to to respond to the client that made the original request.

Transaction results can further resolve into a `TransactionFailure`. Transaction failure objects contain the necessary data to retry the request if necessary, and provides a reference to the connection that the failed request was made on, so that the balancer can mark that connection as inactive.

Transactions are made and complete on a separate thread from what the main balancer runs on, so that socket reading functions like `recv` don't block the main thread unnecessarily.

> [!NOTE]
> Originally, parallelism was attempted through non-blocking sockets, and unix functions like `poll`, but that route was error-prone, with the sockets being unable to read data even if it existed.
> Instead, I set a socket timeout of 10 seconds for `connect`, and an timeout of 60 seconds for `read`. Because of this, setting a server delay of 55 isn't allowed, as 60 or higher second delays would cause the socket to always close while reading, causing the balancer to always send back empty responses to clients.


## Running the Balancer

The load balancer is set up in four steps, a practical example of which is within the `main` function in `main.cpp`, where this set up happens.
1. Construct the `LoadBalancer` object. Here you pass in different configuration arguments.
2. Use `LoadBalancer::addConnection` to add connections to the balancer. `Connections` are the name of the balancer's *connection* to an underlying server. Each connection requires an IP address, a port, and a weight.
3. Use `LoadBalancer::use` to set the strategy to be used for load balancing. Strategy representations are represented through the `Strategy` enum, and specify how the balancer will distribute clients.
4. Use `LoadBalancer::start` to begin the application. This is a blocking operation.

The balancing is handled within one of the balancer's `start[Strategy]` methods.
All of these functions follow the same general structure.
1. Go through all the current existing transactions, resolving any that have been completed:
   - for successful transactions forward the data back to the original client 
   - for failed transactions, prepare them for retransmission, unless they've already been retried too many times, in which case discard the request and respond to the client with an HTTP 503 error
2. Test any stale connections for inactivity using `LoadBalancer::testServers`.
3. Check for any failed transactions or new requests made to the load balancer, prioritizing theformer. If any are found, move on to the next step, otherwise return to step 1.
4. Using whichever strategy is chosen, select the backing server to send the request to, then send the request and create a transaction for it through the `LoadBalancer::createTransaction` method.

The following load balancer strategies were implemented:

### Weighted Round Robin

Each backing server is assigned a postive integer weight. The load balancer will send requests to that server until the amount of requests sent is equal to the server's weight, then it will move on to the next server in its list, and cycle.

For example:
If server A has a weight of 2, and server B has a weight of 3, the load balancer will send the first two requests it receives to server A, then the next three to server B, then the next two to server A, and so on.

### Least Connections

The load balancer keeps track of the number of transactions that it has in progress with its backing servers, sending new requests to the server with the least current load.

For example:
If server A (with a weight of 3) has 3 in-progress transactions, server B (with a weight of 2) has 5, and server C (with a weight of 2) has 1.
1. The first request the load balancer receives goes to server C.
2. The second request will also go to server C.
3. Since server A and server C are tied in requests, the load balancer will default to the server with the highest weight. In this case, server A.
4. Say that 3 of server B's transactions complete between steps 3 and 4. Then, server B has the fewest ongoing connections (2), and so the next request will go there.  

### Random

Backing servers are picked randomly and uniformly from the list.

## Testing Stale Servers

The goal of the `LoadBalancer::testServers` method is two-fold:
- Find any active connections that have gone down
- Find any inactive connections that have come back up

Performing this on every server all the time is expensive, both for the backing servers, and for the load balancer (especially as the number of backing servers increase), so instead the balancer makes use of real requests made by clients as an easy way to check the connection status. 

As such, the balancer only needs to query connections that have gone *stale*, that is, no requests have been made to them for some period of time.
The server will send an HTTP HEAD request to a stale connection. If it responds, it's still up. If the connection times out, the connection is (most likely) down and can be marked inactive. Once this request is made, whatever the response is, the stale countdown is reset for the connection, as a request was made against it.

However, since its possible for servers that have been killed to be brought back to life and for links that have gone down to come back up, the client will make the same request to every stale inactive server as well. Since inactive servers are never queried in normal operation, this essentially means that we check if these servers have come back up every `X` seconds.

> [!Note]
> Implementation-wise, these "is-alive" transactions are managed separately from other "real" transactions. 
> `testServers` handles both creating these transactions and resolving them.

## `Server`

This class sets up a simple TCP server on a given port number. 

The server is set up when it is constructed, creating a socket listening to a port.

The process for using the server is:
1. Call `Server::tryAcceptLatest`. This method will listen to the local server socket for `timeout` milliseconds, waiting for any connections. If a connection is found, the method returns the data that it has collected.
This data is wrapped up in the struct `AcceptData`, and bundled with the number of the remote socket's file descriptor.
   > [!IMPORTANT]
   > The returned file descriptor is only a reference, and shouldn't be closed or reinitalized using a `FileDescriptor` or `Socket` class. Handling the socket connection for clients should remain the sole responsibility of the `Server` class.

2. Perform any actions you need with the received request from the server.

3. Using `Server::respond`, send back a string response to the socket's file descriptor. 
   > [!IMPORTANT]
   > This method also closes the socket connection, causing the held file descriptor number to become invalid.

## `TcpClient`

This class manages querying backend servers at a specific IP and port. 

On calling `TcpClient::query`, the class will create a socket connection to the address that it is set up for and query it for data. The class will timeout after 10 seconds if it cannot connect, and 60 seconds when reading data. The `query` function returns a string response containing data received from the remote connection, and nothing if the connection or reading process failed.
