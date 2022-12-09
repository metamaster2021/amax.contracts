p=realme.dao
c=arm.mblchk
c2=arm.didchker
c3=rm.qachker
r=realme.owner
p1=amaxamaxa2p2
u1=amaxamaxa2u2


# p=realme.dao1
# c=arm.mblchk1
# c2=arm.didchker1
# c3=rm.qachker1
# r=realme.owner1
# p1=amaxamaxa2p2
# u1=amaxamaxa2u2

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
tcli push action $r init '[70, "'$p'"]' -p $r

tcli set account permission ${p} active --add-code
tcli set account permission ${c} active --add-code
tcli set account permission ${c2} active --add-code
tcli set account permission ${c3} active --add-code

tcli set account permission ${r} active --add-code


a=arm.admin
tnew $a

#5KffM7EnR4vypXvC8N4U6fwGGebtf6HKFYtfeqECxA8CQqt44Vy

tcli push action $c setchecker '["'$a'",  ["newaccount","bindinfo","bindanswer","createcorder","setscore"] ]' -p $c
tcli push action $c3 setchecker '["'$a'",  ["newaccount","bindinfo","bindanswer","createorder","setscore"] ]' -p $c3
tcli push action $c2 setchecker '["'$a'",  ["newaccount","bindinfo","bindanswer","createorder","setscore"] ]' -p $c2


tcli push action $r addauditconf '["'$c'", "mobileno", ["0.00000000 AMAX", "title:'$c'","desc:'$c'", "url:'$c'",3, true, "running"] ]' -p $r
tcli push action $r addauditconf '["'$c2'", "safetyanswer", ["0.00000000 AMAX", "title:'$c'","desc:'$c'", "url:'$c'",3, false, "running"] ]' -p $r
tcli push action $r addauditconf '["'$c3'", "did", ["0.00000000 AMAX", "title:'$c3'","desc:'$c3'", "https://did-dev.ambt.art",3, false, "running"] ]' -p $r

amcli -u http://hk-t1.nchain.me:18887 create key  --to-console

tcli push action $c newaccount '["'$a'", "'$p'","'$p1'", "mobieinfo", {"threshold":1,"keys":[{"key":"AM6baM8PU6YYt5ckf5nLWN9bSaSjBY7dmZaDRJ1aPssqmvQgmBhn","weight":1}],"waits":[],"accounts":[]}]' -p $a

tcli push action $c newaccount '["'$a'", "'$p1'","'$u1'",  "mobieinfo", {"threshold":1,"keys":[{"key":"AM7xU3pJTt7jkhYFWZE7ryZFYFxy6ML3nNvHXXa2oxhsnVVhsnoa","weight":1}],"waits":[],"accounts":[]}]' -p $a


amcli wallet import -n amtest --private-key 5KPQXuapHyviEDtCLCwso6m6UfkXvCpDjyAofQCdiQBgX9Jf3C4


tcli push action $r addauth '["'$u1'", "'$c2'"]' -p $u1
tcli push action $c2 bindinfo '["'$a'", "'$u1'", "answer_hash"]' -p $a



tcli push action $c2 createorder '[111111,"'$c2'", "'$u1'",false, 3, ["public_key","AM8buP5bVhA2uvNgHRcdHLnvYUwauVnF5HTe1nBSVJWV9s37NRuV"]]' -p $c2
tcli push action $c setscore '["'$c'","'$u1'", 1, 3]' -p $c

tcli push action $r closeorder '["'$c2'", 1]' -p $c2



