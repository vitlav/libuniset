#!/bin/sh

ulimit -Sc 1000000

#for i in `seq 1 20`; 
#do
	uniset-start.sh -f ./uniset-simitator --confile test.xml --sid 10,16 
#done

#wait

#--unideb-add-levels info,crit,warn,level9,system
