#!/bin/bash
set -e # exit on failure of any "simple" command (excludes &&, ||, or | chains)
# expected places to find AMAX CMAKE in the docker container, in ascending order of preference
[[ -e /usr/lib/eosio/lib/cmake/eosio/eosio-config.cmake ]] && export CMAKE_FRAMEWORK_PATH="/usr/lib/eosio"
[[ -e /opt/eosio/lib/cmake/eosio/eosio-config.cmake ]] && export CMAKE_FRAMEWORK_PATH="/opt/eosio"
[[ ! -z "$AMAX_ROOT" && -e $AMAX_ROOT/lib/cmake/eosio/eosio-config.cmake ]] && export CMAKE_FRAMEWORK_PATH="$AMAX_ROOT"
# fail if we didn't find it
[[ -z "$CMAKE_FRAMEWORK_PATH" ]] && exit 1
# export variables
echo "" >> /amax.contracts/docker/environment.Dockerfile # necessary if there is no '\n' at end of file
echo "ENV CMAKE_FRAMEWORK_PATH=$CMAKE_FRAMEWORK_PATH" >> /amax.contracts/docker/environment.Dockerfile
echo "ENV AMAX_ROOT=$CMAKE_FRAMEWORK_PATH" >> /amax.contracts/docker/environment.Dockerfile