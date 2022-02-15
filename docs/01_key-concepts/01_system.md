---
content_title: System contracts, system accounts, privileged accounts
link_text: System contracts, system accounts, privileged accounts
---

At the genesis of an AMAX-based blockchain, there is only one account present, `amax` account, which is the main `system account`. There are other `system account`s, created by `amax` account, which control specific actions of the `system contract`s [mentioned in previous section](../#system-contracts-defined-in-amax.contracts). __Note__ the terms `system contract` and `system account`. `Privileged accounts` are accounts which can execute a transaction while skipping the standard authorization check. To ensure that this is not a security hole, the permission authority over these accounts is granted to `amax.prods` system account.

As you just learned the relation between a `system account` and a `system contract`, it is also important to remember that not all system accounts contain a system contract, but each system account has important roles in the blockchain functionality, as follows:

|Account|Privileged|Has contract|Description|
|---|---|---|---|
|amax|Yes|It contains the `amax.system` contract|The main system account on an AMAX-based blockchain.|
|amax.msig|Yes|It contains the `amax.msig` contract|Allows the signing of a multi-sig transaction proposal for later execution if all required parties sign the proposal before the expiration time.|
|amax.wrap|Yes|It contains the `amax.wrap` contract.|Simplifies block producer superuser actions by making them more readable and easier to audit.|
|amax.token|No|It contains the `amax.token` contract.|Defines the structures and actions allowing users to create, issue, and manage tokens on AMAX-based blockchains.|
|amax.names|No|No|The account which is holding funds from namespace auctions.|
|amax.bpay|No|No|The account that pays the block producers for producing blocks. It assigns 0.25% of the inflation based on the amount of blocks a block producer created in the last 24 hours.|
|amax.prods|No|No|The account representing the union of all current active block producers permissions.|
|amax.ram|No|No|The account that keeps track of the SYS balances based on users actions of buying or selling RAM.|
|amax.ramfee|No|No|The account that keeps track of the fees collected from users RAM trading actions: 0.5% from the value of each trade goes into this account.|
|amax.saving|No|No|The account which holds the 4% of network inflation.|
|amax.stake|No|No|The account that keeps track of all SYS tokens which have been staked for NET or CPU bandwidth.|
|amax.vpay|No|No|The account that pays the block producers accordingly with the votes won. It assigns 0.75% of inflation based on the amount of votes a block producer won in the last 24 hours.|
|amax.rex|No|No|The account that keeps track of fees and balances resulted from REX related actions execution.|
