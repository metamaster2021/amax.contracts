p=proxyprox2p1
c=checkchec2c1
c2=checkchec2c2
c3=checkdidd2c3
r=recoveryr2c1
p1=amaxamaxa2p1
u1=amaxamaxa2u1

tnew $p
tnew $c
tnew $c2
tnew $c3
tnew $r



tset $p amax.proxy
tset $c amax.checker
tset $c2 amax.checker
tset $c3 amax.checker
tset $r amax.recover


tcli push action $p init '["'$r'"]' -p $p
tcli push action $c init '["'$r'", "'$p'"]' -p $c
tcli push action $c2 init '["'$r'", "'$p'"]' -p $c2
tcli push action $c3 init '["'$r'", "'$p'"]' -p $c3
tcli push action $r init '[5, "'$p'"]' -p $r

tcli set account permission ${p} active --add-code
tcli set account permission ${c} active --add-code
tcli set account permission ${c2} active --add-code
tcli set account permission ${c3} active --add-code

tcli set account permission ${r} active --add-code


a=proxyprox111

a=proxyprox111
tcli push action $c setchecker '["proxyprox111",  ["newaccount","bindinfo","bindanswer","createcorder","setscore"] ]' -p $c
tcli push action $c3 setchecker '["proxyprox111",  ["newaccount","bindinfo","bindanswer","createorder","setscore"] ]' -p $c3
tcli push action $c2 setchecker '["proxyprox111",  ["newaccount","bindinfo","bindanswer","createorder","setscore"] ]' -p $c2


tcli push action $r addauditconf '["'$c'", "mobileno", ["0.00000000 AMAX", "title:'$c'","desc:'$c'", "url:'$c'",3, true, "running"] ]' -p $a
tcli push action $r addauditconf '["'$c2'", "safetyanswer", ["0.00000000 AMAX", "title:'$c'","desc:'$c'", "url:'$c'",3, false, "running"] ]' -p $a
tcli push action $r addauditconf '["'$c3'", "did", ["0.00000000 AMAX", "title:'$c3'","desc:'$c3'", "https://did-dev.ambt.art/compare",3, false, "running"] ]' -p $r

amcli -u http://hk-t1.nchain.me:18887 create key  --to-console

tcli push action $c newaccount '["'$c'", "'$p'","'$p1'", "mobieinfo", {"threshold":1,"keys":[{"key":"AM6baM8PU6YYt5ckf5nLWN9bSaSjBY7dmZaDRJ1aPssqmvQgmBhn","weight":1}],"waits":[],"accounts":[]}]' -p $c


tcli push action $c newaccount '["'$c'", "'$p1'","'$u1'", {"threshold":1,"keys":[{"key":"AM6aDi6VcBRtNC1uowzdNDQr6VRD6JvzNjmQrwLkEyQAnv4FBWHH","weight":1}],"waits":[],"accounts":[]}, "mobieinfo"]' -p $c


amcli wallet import -n amtest --private-key 5JrWzRdZUjFt3v3EyD2tDY8WAHJQYP6saTqMA93uS3a4QgTGfRR


tcli push action $r addauth '["'$u1'", "'$c2'"]' -p $u1
tcli push action $c2 bindinfo '["'$c2'", "'$u1'", "answer_hash"]' -p $c2



tcli push action $c2 createorder '[111111,"'$c2'", "'$u1'",false, 3, ["public_key","AM8buP5bVhA2uvNgHRcdHLnvYUwauVnF5HTe1nBSVJWV9s37NRuV"]]' -p $c2
tcli push action $c setscore '["'$c'","'$u1'", 1, 3]' -p $c

tcli push action $r closeorder '["'$c2'", 1]' -p $c2



