#!/usr/bin/env python

from argparse import ArgumentParser
from mininet.net import Mininet
from mininet.node import Controller
from mininet.cli import CLI
from mininet.log import setLogLevel, info

def simple_load_balancer(num_clients = 6, num_servers = 4, delay = 0):
    """Create a topology with clients, servers, and a load balancer all connected up to a single switch.

    Args:
        clients (int, optional): The number of clients to connect. Defaults to 6.
        servers (int, optional): The number of hosts to connect. Defaults to 4.
    """

    net = Mininet( controller=Controller )

    info( '*** Adding controller\n' )
    net.addController( 'c0' )

    info( '*** Adding clients\n' )
    clients = [net.addHost(f'h{i}', ip=f'10.0.0.{i}') for i in range(1, num_clients + 1)]
    info( '*** Adding servers\n' )
    servers = [net.addHost(f'h{i}', ip=f'10.0.1.{i}') for i in range(num_clients + 1, num_clients + num_servers + 1)]
    info( '*** Adding load balancer\n' )
    lb = net.addHost(f'h{num_clients + num_servers + 1}', ip='10.0.2.1')


    info( '*** Adding switch\n' )
    switch = net.addSwitch( 's1', ip=f'10.0.2.1' )

    info( '*** Creating links\n' )
    for client in clients:
        net.addLink(client, switch)

    for server in servers:
        net.addLink(server, switch)

    net.addLink(lb, switch)

    info( '*** Starting network\n')
    net.start()


    info( f'*** Spinning up webservers on servers (h{num_clients + 1} - h{num_clients + num_servers}) \n')
    for server in servers:
        server.cmdPrint(f'python ./mininet/server.py -l 0.0.0.0 -p 80 -s {delay} {server.name} 2> {server.name}_logs.txt &')

    info( f'*** Launching load balancer (h{num_clients + num_servers + 1}) \n')
    
    arg_list = []
    for server in servers:
        arg_list.append(server.IP())
        arg_list.append('80')
        arg_list.append('1')
    
    lb.cmdPrint('sudo ./build/bin/Load_Balancer -p 80 -c 30 ' + ' '.join(arg_list) + ' 2> lb_logs.txt &')
    
    info( '*** Running CLI\n' )
    CLI( net )

    info( '*** Stopping network' )
    net.stop()


if __name__ == '__main__':
    parser = ArgumentParser(description="Run a simple HTTP server")
    parser.add_argument(
        "-c",
        "--clients",
        type=int,
        default=6,
        help="Specify the number of clients in this topo.",
    )
    parser.add_argument(
        "-s",
        "--servers",
        type=int,
        default="4",
        help="Specify the server on which the server listens",
    )
    parser.add_argument(
        "-d",
        "--delay",
        type=int,
        default=0,
        help="Specify in seconds on how long the each server takes to respond",
    )
    
    args = parser.parse_args()
    
    setLogLevel( 'info' )
    simple_load_balancer(args.clients, args.servers, args.delay)