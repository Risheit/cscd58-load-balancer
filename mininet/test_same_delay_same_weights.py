#!/usr/bin/env python

from argparse import ArgumentParser
from concurrent.futures import ThreadPoolExecutor
import statistics
from time import time
from mininet.net import Mininet
from mininet.node import Controller
from mininet.log import setLogLevel, info

def test_command_time(client):
    start = time()
    client.cmd("time curl 10.10.10.10 & " * 3 + "wait")
    print(f"client {client.name} has finished all of its queries.")
    end = time()
    return end - start

def simple_load_balancer(args):
    num_clients = 10
    num_servers = 10
    
    net = Mininet( controller=Controller )

    info( '*** Adding controller\n' )
    net.addController( 'c0' )

    info( '*** Adding clients\n' )
    clients = [net.addHost(f'h{i + 1}',) for i in range(1, num_clients + 1)]
    info( '*** Adding servers\n' )
    servers = [net.addHost(f'h{i + 1}',) for i in range(num_clients + 1, num_clients + num_servers + 1)]
    info( '*** Adding load balancer\n' )
    lb = net.addHost(f'h1', ip='10.10.10.10')


    info( '*** Adding switch\n' )
    switch = net.addSwitch('s1')

    info( '*** Creating links\n' )
    for client in clients:
        net.addLink(client, switch)

    for server in servers:
        net.addLink(server, switch)

    net.addLink(lb, switch)

    info( '*** Starting network\n')
    net.start()


    info( f'*** Spinning up webservers on servers (h{num_clients + 1} - h{num_clients + num_servers}) \n')
    for (i, server) in enumerate(servers):
        server.cmdPrint(f'python ./mininet/server.py -l 0.0.0.0 -p 80 -d 1 {server.name} &')

    info( f'*** Launching load balancer ({lb.name}) \n')
    
    arg_list = []
    for server in servers:
        arg_list.append(server.IP())
        arg_list.append('80')
        arg_list.append('1')
    
    lb.cmdPrint(f'sudo ./build/bin/Load_Balancer --{args.strategy} -p 80 -c 100 -t 30 ' + ' '.join(arg_list) + ' 2> logs.txt &')
    
    info( '*** Testing curls to each server' )
    with ThreadPoolExecutor() as executor:
        futures = [executor.submit(test_command_time, client) for client in clients]

    info( '*** Statistics: ')
    times = [future.result() for future in futures]
    total_time = sum(times)
    mean = statistics.fmean(times)
    median = statistics.median(times)
    
    print("total time: ", total_time, " sec")
    print("mean: ", mean, " sec")
    print("median: ", median, " sec")
    


    info( '*** Stopping network' )
    net.stop()


if __name__ == '__main__':
    parser = ArgumentParser(description="Boot up Mininet with a topology with clients, "
                            +"servers, and a load balancer all connected up to a single switch.")
    parser.add_argument(
        "-t",
        "--strategy",
        type=str,
        default="robin",
        help="The strategy to use. The argument for this option is passed in as [--strategy] to the load balancer."
    )
    
    args = parser.parse_args()
    
    setLogLevel( 'info' )
    simple_load_balancer(args)