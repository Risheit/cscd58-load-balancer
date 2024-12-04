#!/usr/bin/env python

from argparse import ArgumentParser
from mininet.net import Mininet
from mininet.node import Controller
from mininet.cli import CLI
from mininet.log import setLogLevel, info

def simple_load_balancer(args):
    """Create a topology with clients, servers, and a load balancer all connected up to a single switch.

    Args:
        clients (int, optional): The number of clients to connect. Defaults to 6.
        servers (int, optional): The number of hosts to connect. Defaults to 4.
    """

    net = Mininet( controller=Controller )

    info( '*** Adding controller\n' )
    net.addController( 'c0' )

    info( '*** Adding clients\n' )
    clients = [net.addHost(f'h{i}') for i in range(1, args.clients + 1)]
    info( '*** Adding servers\n' )
    servers = [net.addHost(f'h{i}') for i in range(args.clients + 1, args.clients + args.servers + 1)]
    info( '*** Adding load balancer\n' )
    lb = net.addHost(f'h{args.clients + args.servers + 1}', ip='10.10.10.10')


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


    info( f'*** Spinning up webservers on servers (h{args.clients + 1} - h{args.clients + args.servers}) \n')
    for server in servers:
        server.cmdPrint(f'python ./mininet/server.py -l 0.0.0.0 -p 80 -d {args.delay} {server.name} &')

    info( f'*** Launching load balancer (h{args.clients + args.servers + 1}) \n')
    
    arg_list = []
    for server in servers:
        arg_list.append(server.IP())
        arg_list.append('80')
        arg_list.append('1')
    
    lb.cmdPrint(f'sudo ./build/bin/Load_Balancer --{args.strategy} -p 80 -c 30 --stale {args.stale} --log {args.log} ' + ' '.join(arg_list) + ' 2> logs.txt &')
    
    info( '*** Running CLI\n' )
    CLI( net )

    info( '*** Stopping network' )
    net.stop()


if __name__ == '__main__':
    parser = ArgumentParser(description="Boot up Mininet with a topology with clients, "
                            +"servers, and a load balancer all connected up to a single switch.")
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
        help="Specify the server on which the server listens.",
    )
    parser.add_argument(
        "-d",
        "--delay",
        type=int,
        default=0,
        help="Specify in seconds on how long the each server takes to respond." 
        + "The argument for this option is passed in as [-s DELAY] to the servers.",
    )
    parser.add_argument(
        "-r",
        "--strategy",
        type=str,
        default="robin",
        help="The strategy to use. The argument for this option is passed in as [--strategy] to the load balancer."
    )
    parser.add_argument(
        "-t",
        "--stale",
        type=int,
        default=60,
        help="Timeout before the balancer checks if a server is active."
    )
    parser.add_argument(
        "--log",
        type=int,
        default=3,
        help="The logging level of the balancer."
    )
    args = parser.parse_args()
    
    setLogLevel( 'info' )
    simple_load_balancer(args)