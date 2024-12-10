#!/usr/bin/env python

from argparse import ArgumentParser
from concurrent.futures import ThreadPoolExecutor
import math
import statistics
import random
from time import sleep, time
from mininet.net import Mininet
from mininet.node import Controller
from mininet.log import info
from mininet.cli import CLI


def test_command_time(client):
    sleep(random.uniform(0, 10)) # Random sleeping to simulate people connecting and disconnecting
    start = time()
    client.cmd("time curl 10.10.10.10 & " * 10 + "wait")
    info(f"client {client.name} has finished all of its queries.\n")
    end = time()
    return end - start

def test_loadbalancer(strategy, min_delays, max_delays, weights, num_clients = 100, num_servers = 10):
    
    net = Mininet( controller=Controller )

    info( '*** Adding controller\n' )
    net.addController( 'c0' )

    info( '*** Adding clients\n' )
    clients = [net.addHost(f'h{i + 1}',) for i in range(1, num_clients + 1)]
    info( '*** Adding servers\n' )
    servers = [net.addHost(f'h{i + 1}',) for i in range(num_clients + 1, num_clients + num_servers + 1)]
    info( '*** Adding load balancer\n' )
    lb = net.addHost(f'h1', ip='10.10.10.10')


    info( '*** Adding switches and connecting links in a linear topology\n' )
    num_switches = math.ceil((num_clients + num_servers + 1) / 20)
    switches = [net.addSwitch(f's{i}') for i in range(1, num_switches)]

    current = iter(switches)
    current_switch = next(current)
    for (i, host) in enumerate([lb] + clients + servers):
        net.addLink(host, current_switch)
        if i != 0 and i % 20 == 0:
            try:
                next_switch = next(current)
            except StopIteration:
                continue
            net.addLink(current_switch, next_switch)
            current_switch = next_switch

    info( '*** Starting network\n' )
    net.start()


    info( '*** Spinning up webservers\n' )
    for (i, server) in enumerate(servers):
        server.cmdPrint(f'python ./mininet/random_server.py -l 0.0.0.0 -p 80 -m {min_delays[i]} -M {max_delays[i]} {server.name} &') 

    info( '*** Launching load balancer\n' )
    
    arg_list = []
    for (i, server) in enumerate(servers):
        arg_list.append(server.IP())
        arg_list.append('80')
        arg_list.append(str(weights[i]))

    # Servers can't go down during testing, and chances that they so bogged down they get marked inactive are low.
    # However, we're not testing stale connections here, so set the stale timer really high to avoid any possible accidentals.
    lb.cmdPrint(f'sudo ./build/bin/Load_Balancer --{strategy} -p 80 -c 30 --stale 9999 --log 3 ' + ' '.join(arg_list) + ' 2> logs.txt &')
    
    sleep(2) # It takes a second for the servers and balancer to spin up in the background, and 
             # there's no good way for waiting until they're ready to accept connections, so just sleep for a few seconds
    
    info( '*** Testing curls to each server\n' )
    with ThreadPoolExecutor() as executor:
        futures = [executor.submit(test_command_time, client) for client in clients]
    
    info( '\n*** Statistics:\n' )
    times = [future.result() for future in futures]
    total_time = sum(times)
    mean = statistics.fmean(times)
    median = statistics.median(times)
    
    info("total time accumulated in waiting: ", total_time, " sec\n")
    info("mean time for clients: ", mean, " sec\n")
    info("median time for clients: ", median, " sec\n\n")

    # info( '*** Running CLI\n' )
    # CLI( net )    

    info( '*** Stopping network\n' )
    net.stop()

    