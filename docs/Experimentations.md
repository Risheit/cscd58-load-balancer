# Experimentations

## Setup

This document discusses the findings of the three different load balancer tests run on a simulated network on mininet.

Each test examines the load balancer efficiency when using different strategies under different backing server conditions.

Between each test, the following details were the same:
- Each test was comprised of 300 clients (each a single host) sending queries to 20 backing servers (each also a single host) through the load balancer (its own host). Each client would wait randomly for 0 - 10 seconds (to simulate more sporadic network traffic), then send 20 HTTP GET requests (via cURL) to the load balancer in parallel.
    > [!NOTE]
    > Python, when handling asynchronous hosts, will only queue a certain number of hosts up to run at a time. Because of this, and as more hosts increase the load on the machine, we're instead sending several HTTP requests from one client in parallel, since there's no limit (it's large enough to not matter) to how many processes one client can run at once.
- The network topology in each case was the same. A linear topology of switches, with each switch connecting 20 hosts each until every host is connected.
- The same Random Server configuration (`random_server.py`) was used for each backing server on the network. This server is based on the standard webserver found in `server.py`. Each server waits between a test-specific mininum and maximum delay before responding with a dummy HTML response. Each server can only process one request at a time; the server only begins processing the next response in its queue after responding to the response it was currently handling.
- Each test calculates the sum total of time it took all clients to finish receiving responses, and the mean and median time it took each client to get a response for all 10 GET requests.
- Other than the load balancer strategy, the load balancer was configured the same each time.

In essence, each test calls the `test_loadbalancer` function found in `loadbalancer_test.py` with different input arguments. As such, each test individually configures:
- The minimum and maximum delay for each backing server
- The weighting of each server configured for the load balancer
- The balancing strategy used by the load balancer

---

The following are the tests ran and the scripts that they correspond to.
Each test was run with all three of our implemented strategies:
- Weighted Round Robin
- Least Connections
- Random Selection


### Testing Different Server Delays and Server Weights

Corresponding to the script `test_diff_delay_diff_weights.py`.

- The minimum server delay was set to `0` seconds for each backing server
- The maximum server delays were `[1, 1, 1, 2, 2, 1, 2, 2, 2, 0.5]`, where each number corresponds to the maximum delay in seconds for two backing servers
- The server weights were `[2, 2, 2, 1, 1, 2, 1, 1, 1, 3]`, where each number corresponds to the weight of two backing servers

### Testing Different Server Delays and Same Server Weights

Corresponding to the script `test_diff_delay_same_weights.py`.

- The minimum server delay was set to `0` seconds for each backing server
- The maximum server delays were `[1, 1, 1, 2, 2, 1, 2, 2, 2, 0.5]`
- The server weights were set to `1` for each backing server

### Testing Same Server Delays and Same Server Weights

Corresponding to the script `test_same_delay_same_weights.py`.

- The minimum server delay was set to `0` seconds for each backing server
- The maximum server delays was set to `1` second for each backing server
- The server weights were set to `1` for each backing server

## Findings

### Testing Different Server Delays and Server Weights

#### Weighted Round Robin Results

- Total time accumulated in waiting:  `1264.732236623764` seconds
- Mean time for clients:  `4.215774122079213` seconds
- Median time for clients:  `3.2200347185134888` seconds

#### Least Connections Results

- Total time accumulated in waiting:  `3901.837819814682` seconds
- Mean time for clients:  `13.00612606604894` seconds
- Median time for clients:  `2.6501195430755615` seconds

#### Random Selection Results

> [!NOTE]
> For random selection, the number of clients was dropped down to 50, as running the test for 300 clients became (predictably) prohibitively slow.

- Total time accumulated in waiting:  `1808.3903965950012`  seconds
- Mean time for clients:  `36.16780793190002`  seconds
- Median time for clients:  `10.556310534477234`  seconds

### Testing Different Server Delays and Same Server Weights

#### Weighted Round Robin Results

- Total time accumulated in waiting:  `1767.9037411212921`  seconds
- Mean time for clients:  `5.893012470404307`  seconds
- Median time for clients:  `3.1835875511169434` seconds

#### Least Connections Results

- Total time accumulated in waiting:  `3363.8876688480377`  seconds
- Mean time for clients:  `11.212958896160126`  seconds
- Median time for clients:  `2.4118988513946533`  seconds

#### Random Selection Results

> [!NOTE]
> For random selection, the number of clients was dropped down to 50, as running the test for 300 clients became (predictably) prohibitively slow.

- Total time accumulated in waiting:  `1471.0547058582306`  seconds
- Mean time for clients:  `29.42109411716461`  seconds
- Median time for clients:  `11.504751205444336`  seconds

### Testing Same Server Delays and Same Server Weights

#### Weighted Round Robin Results

- Total time accumulated in waiting:  `836.1248831748962`  seconds
- Mean time for clients:  `2.787082943916321`  seconds
- Median time for clients:  `1.693103313446045`  seconds

#### Least Connections Results

- Total time accumulated in waiting:  `3170.268858909607`  seconds
- Mean time for clients:  `10.567562863032023`  seconds
- Median time for clients:  `1.7942572832107544`  seconds

#### Random Selection Results

> [!NOTE]
> For random selection, the number of clients was dropped down to 50, as running the test for 300 clients became (predictably) prohibitively slow.

- Total time accumulated in waiting:  `1728.7279238700867`  seconds
- Mean time for clients:  `34.57455847740173`  seconds
- Median time for clients:  `19.406450271606445`  seconds

## Analysis

> [!IMPORTANT]
> There are a few points that need to be acknowledged about server weights:
> - The weights given to the backing servers are static. In reality, they might be adjusted on the fly either algorithmically or manually based on the current states of the backing servers.
> - The weights were assignmed manually by me when setting up the tests, When assigning those weights, I placed larger weights on servers with lower maximum delays, but the value for each weight was chosen somewhat arbitrarily. More accurate server weights, or server weights defined by the load balancer based on some algorithm are likely to perform better than the weights I defined here.

A few interesting trends emerge when looking at the different load balancer strategies in each scenario.

#### Random Selection

There's not a lot to discuss about the random selection strategy. Predictably, it's easily the worst between the three strategies tested, with mean and median client times up to five times worse than the least connections (LC) or weighted round robin (WRR) strategies.

#### Weighted Round Robin

In each case, WRR had the smallest mean time and total overall time by a large margin, while least connections had the smallest median time in both cases where servers had different maximum response delay ranges.
The mean and median were similar, with a few second difference in each case.

WRR performed the best in the simplest case where the weights and delay range for each backing server was the same, with a mean time of around 2.8 seconds, and a median of around 1.7 seconds. 
This increase in speed can be partially attributed to the fact that compared to the other tests the overall maximum delay for each server was smaller, as evidenced by the far smaller total time required for each client to complete their queries, only taking around 836 seconds compared to 1200-1800 seconds required to complete other tests.
However, this decrease in average time likely also likely stems from the fact that in a situtation like this test, round robin balancing becomes a very intuitive solution. In this case where each server has roughly the same expected delay, it only makes sense to equally share the load among each of them, sending roughly the same amount of queries to each backing server, which is exactly what the WRR strategy was doing in this case.

WRR had the worst mean time when we had different server delays, while keeping each server weight at once. This also makes sense, as when different servers have different expected delays, distributing the load perfectly equally between them will lead to a suboptimal solution. 

#### Least Connections

The mean time for LC in every case was far greater than the mean time for WRR, while the median time was smaller, except when server delays and server weights were all the same.
In fact, the large disparity between the mean and median times for the LC strategy in each test points to the fact that there existed a few outlier clients with very large wait times blowing up the mean wait time for LC.

Likely, the reason for this comes from cases where a single backing server is overloaded with requests, causing a massive increase in wait time for the clients connected to that server. During our test, we calculate that client's delay as the total time taken to resolve our 10 HTTP requests that client makes. As these requests are made in parallel, in the case that all of them are routed to the same server, that clients recorded delay blows up.

I hypothesize a possible reason that these outliers might occur:
due to random chance, a server with a greater expected delay finds itself having the current least connections. (Perhaps, due to random chance, it had a string of low delay responses, and cleared out its queue relatively quickly.) Once those new queries (likely from the same client) are routed to it, if that server has a string of high delay responses, it ends up greatly increasing the recorded delay of that client, causing the outlier clients that affect the mean result of the LC strategy.

These outlier issues also likely exist in WRR (the mean time in each test is always higher than the median), but are far, far, less of a problem as requests from a single client are almost always guaranteed, at least partially based on server weights, to be distributed over different servers, reducing the likelihood that a single client will have to deal with multiple long delay requests.

### Conclusions

From these tests, we can come to the rough conclusions that:
- WRR is the most stable of the three tested load balancer strategies. In each case, the median and mean delays are roughly the same, and results are similar over each case.
- Excepting the case where all servers have the same expected delay, LC is in general the faster balancing strategy, but is prone an issue where all the requests made by a client are pushed to the same server, creating a scenario where rarely the receiving a response ends up taking far longer than if we had used WRR.
- Random selection is a bad load balancing strategy.

we can propose a possible optimization of the LC strategy that might mitigate the problem we're seeing. 
We could:
- Keep track of requests from the same client, and don't route them all to the same server
- Keep track of requests being made to a server, and don't allow a server to be sent more than `X` requests in a row.
