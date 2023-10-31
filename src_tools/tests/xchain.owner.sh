
xchainowner=xchain.o4

tnew $xchainowner

tset $xchainowner xchain.owner
tcli set account permission $xchainowner active --add-code


tcli push action $xchainowner init '["joss","joss","joss","0.00010000 AMAX","0.00040000 AMAX"]' -p $xchainowner

tcli push action $xchainowner proposebind '[joss,eth,123322221,"ethpub3234","abcdaccdabcd","xchain.o4","AM72X6YvCeaBYtXnL9Q6fnxGT3DTLdUK8wrTMTJTknsVmVBWC9pu","AM7xtTGXygtpqSwpR9SteL6moV1hMq7m5ULJpQBKZLNJxTEktwmB"]' -p joss


tcli push action $xchainowner proposebind '[joss,eth,123321,"ethpub""AM72X6YvCeaBYtXnL9Q6fnxGT3DTLdUK8wrTMTJTknsVmVBWC9pu","abcdabcdabcd"]' -p joss


tcli get table $xchainowner eth xchainaccts



tcli push action  amax updateauth '{"account": "eth", "permission": "active", "parent": "owner", "auth": {"threshold": 1, "keys": [], "waits": [], "accounts": [{"weight":1,"permission":{"actor":"xchain.o4","permission":"active"}}]}}' -p eth

tcli push action  amax updateauth '{"account": "bsc", "permission": "active", "parent": "owner", "auth": {"threshold": 1, "keys": [], "waits": [], "accounts": [{"weight":1,"permission":{"actor":"xchain.o4","permission":"active"}}]}}' -p bsc




tcli push action $xchainowner proposebind '[joss,eth,123322221,"ethpub233","a12.eth","eth","AM5pMsNrKooHhpDZ2zUojWJWibgKx3vsfEQphXjFKMnQJeedT4RV","AM51655R2X4zVH8cjgXYYFMVhySacY1AcQuwuECgNW8dVzLVVAg9"]' -p joss



tcli push action $xchainowner proposebind '[joss,eth,123322221,"ethpub233","a13.eth","eth","AM8JpA4yBLZKqi4ciQsqHna9Ch2NspSnLRBqWuYWJGVdG9CnJbNy","AM51655R2X4zVH8cjgXYYFMVhySacY1AcQuwuECgNW8dVzLVVAg9"]' -p joss

tcli push action $xchainowner proposebind '[joss,eth,123322221,"ethpub233","a21.eth","eth","a13.eth","AM8JpA4yBLZKqi4ciQsqHna9Ch2NspSnLRBqWuYWJGVdG9CnJbNy"]' -p joss