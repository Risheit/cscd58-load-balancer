#!/usr/bin/env python

from argparse import ArgumentParser
from loadbalancer_tests import test_loadbalancer
from mininet.log import setLogLevel, info

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
    min_delays = [0] * 10
    max_delays = [1] * 10
    weights =    [1] * 10
    test_loadbalancer(args.strategy, min_delays, max_delays, weights, 500, 10)