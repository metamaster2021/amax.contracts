
xchainowner=xchain.o2

tnew $xchainowner

tset $xchainowner xchain.owner
tcli set account permission $xchainowner active --add-code


tcli push action $xchainowner init '["joss","joss","joss","0.00010000 AMAX","0.00040000 AMAX"]' -p $xchainowner

tcli push action $xchainowner proposebind '[joss,eth,123321,"ethpub""AM72X6YvCeaBYtXnL9Q6fnxGT3DTLdUK8wrTMTJTknsVmVBWC9pu","abcdabcdabcd"]' -p joss


tcli push action $xchainowner proposebind '[joss,eth,123321,"ethpub""AM72X6YvCeaBYtXnL9Q6fnxGT3DTLdUK8wrTMTJTknsVmVBWC9pu","abcdabcdabcd"]' -p joss