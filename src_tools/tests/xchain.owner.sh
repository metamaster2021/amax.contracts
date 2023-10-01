
xchainowner=xchain.o1

tnew $xchainowner

tset $xchainowner xchain.owner

tcli push action $xchainowner init '["joss","joss","joss","0.00100000 AMAX","0.00400000 AMAX"]' -p $xchainowner

tcli push action $xchainowner proposebind '[joss,eth,123321,[1,[["AM72X6YvCeaBYtXnL9Q6fnxGT3DTLdUK8wrTMTJTknsVmVBWC9pu",1]],[],[]]]' -p binda1

