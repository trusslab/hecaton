ssh -p 10022 -i images/stretch.img.key root@localhost mkdir /root/$1
scp -P 10022 -i images/stretch.img.key bugs/$1/poc.o root@localhost:/root/$1/

