acct=$1

amcli -u http://120.77.177.218 push action amax.custody \
   planreceiver "'[1,""$acct""]'" -p amax

