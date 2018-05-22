#!/bin/sh -x

IFACE=en0

airmon-ng check kill
rfkill unblock all
airmon-ng start ${IFACE}
ip link set up dev ${IFACE}mon

sh wifi-probe.py ${IFACE}mon
