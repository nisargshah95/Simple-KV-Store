./cmake/build/keyvaluestore/kvserver ./logs.txt &
echo Wait 15s for server initialization
sleep 15
./cmake/build/keyvaluestore/kvclient -e 10000 -n 20000 -w 50 -t 10 -s 0.0.0.0:50051 
pkill kvserver

