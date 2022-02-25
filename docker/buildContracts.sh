#!/bin/bash
set -e # exit on failure of any "simple" command (excludes &&, ||, or | chains)
cd /amax.contracts
./build.sh -c /usr/opt/amax.cdt -e /opt/eosio -t -y
cd build
tar -pczf /artifacts/contracts.tar.gz *
