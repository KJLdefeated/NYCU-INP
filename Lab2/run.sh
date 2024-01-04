curl https://inp.zoolab.org/binflag/challenge?id=110652019 --output demo.bin
flag=$(./unpacker)
flag=`echo $flag | tr -d ' '`
echo "$flag"
response=$(curl -s https://inp.zoolab.org/binflag/verify?v=$flag)
echo "$response"