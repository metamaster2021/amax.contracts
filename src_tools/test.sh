p=realme.owner
#5JZaPBTNwjBeN8N9oRrS479NNJYP2nHhK7FJHNzdSg9ZEsk8M5V
r=realme.dao
#5J8vd1kjKrUvuajJEiu2enTxVq3KUCcB8pwgnePkUSR1tfw1N5y
c=arm.mblchk
#5Kk6Bur5feB495Dj6tnZ8wrVVFmqxQUAu59477v6prJ5sUCpF75
c2=arm.didchker
#5KgwdWpx1vPAYiiKZ2ghw6ZWAGqG9cKGxBPnteA5Rjawdv1S1Y7
c3=arm.qachker
#5JfhR76wpBrKjJVkp8hnB453eAUi9fGREL147RmhHS3uZBL9UP7
a=arm.admin
#5KffM7EnR4vypXvC8N4U6fwGGebtf6HKFYtfeqECxA8CQqt44Vy

p1=arminviter11
u1=amaxamaxa2u2

tnew $a
tnew $p
tnew $c
tnew $c2
tnew $c3
tnew $r



tset $p realme.owner
tset $c realme.auth
tset $c2 realme.auth
tset $c3 realme.auth
tset $r realme.dao


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





tcli push action $c setauth '["'$a'",  ["newaccount","bindinfo","bindanswer","createorder","setscore"] ]' -p $c
tcli push action $c3 setauth '["'$a'",  ["newaccount","bindinfo","bindanswer","createorder","setscore"] ]' -p $c3
tcli push action $c2 setauth '["'$a'",  ["newaccount","bindinfo","bindanswer","createorder","setscore"] ]' -p $c2


tcli push action $r addauditconf cc,"mobileno", ["0.00000000 AMAX", "title:'$c'","desc:'$c'", "url:'$c'",3, true, "running"] ]' -p $r
tcli push action $r addauditconf '["'$c3'", "safetyanswer", ["0.00000000 AMAX", "title:'$c'","desc:'$c'", "url:'$c'",3, false, "running"] ]' -p $r
tcli push action $r addauditconf '["'$c2'", "did", ["0.00000000 AMAX", "title:'$c3'","desc:'$c3'", "https://did-dev.ambt.art",3, false, "running"] ]' -p $r

amcli -u http://hk-t1.nchain.me:18887 create key  --to-console

tcli push action $c newaccount '["'$a'", "'$p'","'$p1'", "mobieinfo", {"threshold":1,"keys":[{"key":"AM8QPn2K5CuyhYee1CATE4XJfL4o5qCt888XhxMXhY3ZeNQ9jVa2","weight":1}],"waits":[],"accounts":[]}]' -p $a

tcli push action $c newaccount '["'$a'", "'$p1'","'$u1'",  "mobieinfo", {"threshold":1,"keys":[{"key":"AM7xU3pJTt7jkhYFWZE7ryZFYFxy6ML3nNvHXXa2oxhsnVVhsnoa","weight":1}],"waits":[],"accounts":[]}]' -p $a


amcli wallet import -n amtest --private-key 5KPQXuapHyviEDtCLCwso6m6UfkXvCpDjyAofQCdiQBgX9Jf3C4


tcli push action $r addauth '["'$u1'", "'$c2'"]' -p $u1
tcli push action $c2 bindinfo '["'$a'", "'$u1'", "answer_hash"]' -p $a



tcli push action $c2 createorder '[111111,"'$c2'", "'$u1'",false, 3, ["public_key","AM8buP5bVhA2uvNgHRcdHLnvYUwauVnF5HTe1nBSVJWV9s37NRuV"]]' -p $c2
tcli push action $c setscore '["'$c'","'$u1'", 1, 3]' -p $c

tcli push action $r closeorder '["'$c2'", 1]' -p $c2



