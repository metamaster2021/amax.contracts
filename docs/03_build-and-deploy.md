---
content_title: How to build amax.contracts
link_text: How to build amax.contracts
---

## Preconditions
Ensure an appropriate version of `eosio.cdt` is installed. Installing `eosio.cdt` from binaries is sufficient, follow the [`eosio.cdt` installation instructions steps](https://developers.eos.io/manuals/eosio.cdt/latest/installation) to install it. To verify if you have `eosio.cdt` installed and its version run the following command

```sh
eosio-cpp -v
```

### Build contracts using the build script

#### To build contracts alone
Run the `build.sh` script in the top directory to build all the contracts.

#### To build the contracts and unit tests
1. Ensure an appropriate version of `amax` has been built from source and installed. Installing `amax` from binaries `is not` sufficient. You can find instructions on how to do it [here](https://developers.eos.io/manuals/eos/latest/install/build-from-source) in section `Building from Sources`.
2. Run the `build.sh` script in the top directory with the `-t` flag to build all the contracts and the unit tests for these contracts.

### Build contracts manually

To build the `amax.contracts` execute the following commands.

On all platforms except macOS:
```sh
cd you_local_path_to/amax.contracts/
rm -fr build
mkdir build
cd build
cmake ..
make -j$( nproc )
cd ..
```

For macOS:
```sh
cd you_local_path_to/amax.contracts/
rm -fr build
mkdir build
cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
cd ..
```

### After build:
* If the build was configured to also build unit tests, the unit tests executable is placed in the _build/tests_ folder and is named __unit_test__.
* The contracts (both `.wasm` and `.abi` files) are built into their corresponding _build/contracts/\<contract name\>_ folder.
* Finally, simply use __cleos__ to _set contract_ by pointing to the previously mentioned directory for the specific contract.

# How to deploy the amax.contracts

## To deploy amax.bios contract execute the following command:
Let's assume your account name to which you want to deploy the contract is `testerbios`
```
amcli set contract testerbios you_local_path_to/amax.contracts/build/contracts/amax.bios/ -p testerbios
```

## To deploy amax.msig contract execute the following command:
Let's assume your account name to which you want to deploy the contract is `testermsig`
```
amcli set contract testermsig you_local_path_to/amax.contracts/build/contracts/amax.msig/ -p testermsig
```

## To deploy amax.system contract execute the following command:
Let's assume your account name to which you want to deploy the contract is `testersystem`
```
amcli set contract testersystem you_local_path_to/amax.contracts/build/contracts/amax.system/ -p testersystem
```

## To deploy amax.token contract execute the following command:
Let's assume your account name to which you want to deploy the contract is `testertoken`
```
amcli set contract testertoken you_local_path_to/amax.contracts/build/contracts/amax.token/ -p testertoken
```

## To deploy amax.wrap contract execute the following command:
Let's assume your account name to which you want to deploy the contract is `testerwrap`
```
amcli set contract testerwrap you_local_path_to/amax.contracts/build/contracts/amax.wrap/ -p testerwrap
```