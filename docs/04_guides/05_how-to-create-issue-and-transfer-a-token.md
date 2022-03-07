---
content_title: How to create, issue and transfer a token
link_text: How to create, issue and transfer a token
---

## Step 1: Obtain Contract Source

Navigate to your contracts directory.

```sh
cd CONTRACTS_DIR
```

Pull the source
```sh
git clone https://github.com/armoniax/amax.contracts --branch master --single-branch
```

```sh
cd amax.contracts/contracts/amax.token
```

## Step 2: Create Account for Contract
[[info]]
| You may have to unlock your wallet first!

```shell
amcli create account amax amax.token AM6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
```

## Step 3: Compile the Contract

```shell
eosio-cpp -I include -o amax.token.wasm src/amax.token.cpp --abigen
```

## Step 4: Deploy the Token Contract

```shell
amcli set contract amax.token CONTRACTS_DIR/amax.contracts/contracts/amax.token --abi amax.token.abi -p amax.token@active
```

Result should look similar to the one below:
```console
Reading WASM from ...
Publishing contract...
executed transaction: 69c68b1bd5d61a0cc146b11e89e11f02527f24e4b240731c4003ad1dc0c87c2c  9696 bytes  6290 us
#         amax <= eosio::setcode               {"account":"amax.token","vmtype":0,"vmversion":0,"code":"0061736d0100000001aa011c60037f7e7f0060047f...
#         amax <= eosio::setabi                {"account":"amax.token","abi":"0e656f73696f3a3a6162692f312e30000605636c6f73650002056f776e6572046e61...
warning: transaction executed locally, but may not be confirmed by the network yet         ]
```

## Step 5: Create the Token

```shell
amcli push action amax.token create '[ "amax", "1000000000.0000 SYS"]' -p amax.token@active
```

Result should look similar to the one below:
```console
executed transaction: 0e49a421f6e75f4c5e09dd738a02d3f51bd18a0cf31894f68d335cd70d9c0e12  120 bytes  1000 cycles
#   amax.token <= amax.token::create          {"issuer":"amax","maximum_supply":"1000000000.0000 SYS"}
```

An alternate approach uses named arguments:

```shell
amcli push action amax.token create '{"issuer":"amax", "maximum_supply":"1000000000.0000 SYS"}' -p amax.token@active
```

Result should look similar to the one below:
```console
executed transaction: 0e49a421f6e75f4c5e09dd738a02d3f51bd18a0cf31894f68d335cd70d9c0e12  120 bytes  1000 cycles
#   amax.token <= amax.token::create          {"issuer":"amax","maximum_supply":"1000000000.0000 SYS"}
```
This command created a new token `SYS` with a precision of 4 decimals and a maximum supply of 1000000000.0000 SYS.  To create this token requires the permission of the `amax.token` contract. For this reason, `-p amax.token@active` was passed to authorize the request.

## Step 6: Issue Tokens

The issuer can issue new tokens to the issuer account in our case `amax`.

```sh
amcli push action amax.token issue '[ "amax", "100.0000 SYS", "memo" ]' -p amax@active
```

Result should look similar to the one below:
```console
executed transaction: a26b29d66044ad95edf0fc04bad3073e99718bc26d27f3c006589adedb717936  128 bytes  337 us
#   amax.token <= amax.token::issue           {"to":"amax","quantity":"100.0000 SYS","memo":"memo"}
warning: transaction executed locally, but may not be confirmed by the network yet         ]
```

## Step 7: Transfer Tokens

Now that account `amax` has been issued tokens, transfer some of them to account `bob`.

```shell
amcli push action amax.token transfer '[ "amax", "bob", "25.0000 SYS", "m" ]' -p amax@active
```

Result should look similar to the one below:
```console
executed transaction: 60d334850151cb95c35fe31ce2e8b536b51441c5fd4c3f2fea98edcc6d69f39d  128 bytes  497 us
#   amax.token <= amax.token::transfer        {"from":"amax","to":"bob","quantity":"25.0000 SYS","memo":"m"}
#         amax <= amax.token::transfer        {"from":"amax","to":"bob","quantity":"25.0000 SYS","memo":"m"}
#           bob <= amax.token::transfer        {"from":"amax","to":"bob","quantity":"25.0000 SYS","memo":"m"}
warning: transaction executed locally, but may not be confirmed by the network yet         ]
```
Now check if "bob" got the tokens using [amcli get currency balance](https://developers.eos.io/manuals/eos/latest/amcli/command-reference/get/currency-balance)

```shell
amcli get currency balance amax.token bob SYS
```

Result:
```console
25.00 SYS
```

Check "amax's" balance, notice that tokens were deducted from the account

```shell
amcli get currency balance amax.token amax SYS
```

Result:
```console
75.00 SYS
```