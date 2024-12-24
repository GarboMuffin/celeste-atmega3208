#!/bin/bash

tty0() {
	while true; do
		while [ ! -e "/dev/ttyACM0" ]; do
			sleep 0.01
		done
		putty -serial /dev/ttyACM0
	done
}

tty1() {
	while true; do
		while [ ! -e "/dev/ttyACM1" ]; do
			sleep 0.01
		done
		putty -serial /dev/ttyACM1
	done
}

tty0 & tty1
