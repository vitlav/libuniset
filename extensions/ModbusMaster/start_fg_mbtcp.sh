#!/bin/sh

./uniset-start.sh -f ./uniset-mbtcpmaster \
--confile test.xml \
--mbtcp-name MBMaster1 \
--smemory-id SharedMemory \
--dlog-add-levels info,crit,warn,level4,level3 \
--mbtcp-filter-field mbtcp \
--mbtcp-filter-value 1 \
--mbtcp-gateway-iaddr 127.0.0.1 \
--mbtcp-gateway-port 2048 \
--mbtcp-recv-timeout 5000 \
--mbtcp-force-disconnect 1 \
--mbtcp-polltime 5000 \
--mbtcp-exchange-mode-id MB1_Mode_AS \
--mbtcp-set-prop-prefix tcp_
#--mbtcp-filter-field mbtcp --mbtcp-filter-value 1
