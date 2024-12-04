# On Implementation

This document contains details about the actual implementation of the load balancer.
Notable files and functions have entries here, explaining the code and the reasons behind them.

Smaller utility classes might not have a description here, as they don't relate very much to the topic at hand. If necessary, more information is likely located at the beginning of the object's header file.

## `LoadBalancer`

The main class running the load balancer application.
The `LoadBalancer::start(...)` method is the main entry point of this program, starting up a server that takes in socket data from received connections, and then forwards them to set up backing servers.

The balancer uses several helper structs to wrap the data it handles.

### `Connection`

A wrapper for `TcpClient` to add metadata (provided through the `Metadata` structure). 

A *connection* is the term used within the codebase for the relationship between the balancer and its underlying servers, a general reference for both the backing server and the connecting link between that server and the balancer. 
Connections may *go down* or be considered *inactive* when the balancer stops receiving data from the server for any reason, and are considered *stale* if a period of time (specified when constructing the balancer) passes without any requests made to it. More on this when discussing the `LoadBalancer::testServers` method.

### `Transaction`

A *transaction* is the term used within the codebase for the request and response pair made by the balancer to one of its connections. They are created when the balancer makes the inital request, whatever it may be, and objects hold a `future`, an asynchronous promise that resolves to a `TransactionResult` after either:
- The backing server responds to the request
- The socket the data is made on times out or fails to connect for some reason

Transaction results contain the response received from the server if it exists, along with information about that data, like the connection it belongs to and which socket that the response should be forwarded to to respond to the client that made the original request.

Transaction results can further resolve into a `TransactionFailure`. Transaction failure objects contain the necessary data to retry the request if necessary, and provides a reference to the connection that the failed request was made on, so that the balancer can mark it as inactive.

Transactions are made and complete on a separate thread from what the main balancer runs on, so that socket reading functions like `recv` don't block the main thread unnecessarily.

> [!NOTE]
> Originally, parallelism was attempted through non-blocking sockets, and unix functions like `poll`, but that route was error-prone, with the sockets being unable to read data even if it existed.
> Instead, we set a socket timeout of 10 seconds for `connect`, and an timeout of 60 seconds for `read`. Because of this, setting a server delay of 55 isn't allowed, as 60 or higher second delays would cause the socket to close while reading, causing the balancer to always send back empty responses to clients. 


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
4. Using whichever strategy is chosen, select the backing server to send the request to, and send therequest and create a transaction for it through the `LoadBalancer::createTransaction` method.

For more details about strategies, please see the `docs/On Strategies.md`.

## Testing Stale Servers

The goal of the `LoadBalancer::testServers` method is two-fold:
- Find any active connections that have gone down
- Find any inactive connections that have come back up.

Performing this on every server all the time is expensive, both for the backing servers, and for the load balancer (especially as the number of backing servers increase), so instead the balancer makes use of real requests made by clients as an easy way to check the connection status. 

As such, the balancer only needs to query connections that have gone *stale*, that is, no requests have been made to them for some period of time.
The server will send an HTTP Head request to a stale connection. If it responds, it's still up. If the connection times out, the connection is (most likely) down or inaccessible and can be marked inactive. Once this request is made, whatever the case the stale countdown is reset for the connection, as a request was made against it.

However, since its possible for servers that have been killed to be brought back to life, and for links that have gone down to come back up, the client will make the same request to every stale inactive server as well. Since inactive servers are never queried in reality, this essentially means that we check if these servers have come back up every `X` seconds.

> [!Note]
> Implementation-wise, these "is-alive" transactions are stored and checked separately from other real transactions. 
> `testServers` handles both creating these transactions and resolving them.

## `FileDescriptor`

## `Server`

## `TcpClient`

## `Mininet Scripts`