---
content_title: How to vote
link_text: How to vote
---

# Goal

Vote for a block producer

# Before you begin

* Install the current supported version of amcli

* Ensure the reference system contracts from `amax.contracts` repository is deployed and used to manage system resources

* Understand the following:
  * What is a block producer
  * How does voting works

* Unlock your wallet

# Steps

Assume you are going to vote for blockproducer1 and blockproducer2 from an account called `amaxtestts2`, execute the following:

```bash
amcli system voteproducer prods amaxtestts2 blockproducer1 blockproducer2
```

You should see something like below:

```console
executed transaction: 2d8b58f7387aef52a1746d7a22d304bbbe0304481d7751fc4a50b619df62676d  128 bytes  374 us
#         amax <= eosio::voteproducer          {"voter":"amaxtestts2","proxy":"","producers":["blockproducer1","blockproducer2"]}
```