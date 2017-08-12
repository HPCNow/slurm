#!/bin/bash
#
source /etc/slurm/burst_buffer.conf
echo "Sent $# vars" >> /home/snow/bb_simulator.out
echo "vars: $@" >> /home/snow/bb_simulator.out
