g++ client4.cpp -o ./tmp/4.out
for i in $(seq 1 1 3);
do
    ./tmp/4.out > "./tmp/${i}.txt" &
done