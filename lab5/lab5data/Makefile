MYDEFS = -g -Wall -std=c++11 -DLOCALHOST=\"127.0.0.1\"

split: split.cpp
	g++ ${MYDEFS} split.cpp -o split

lab5b: logging.cpp
	g++ ${MYDEFS} logging.cpp -o lab5b

test:
	rm -f xyzzy xyzzy.log
	./lab5b > xyzzy
	./lab5b xyzzy.log
	diff xyzzy xyzzy.log
	cat xyzzy.log
	xxd -g 1 xyzzy.log

lab5c: md5-calc.cpp
	g++ ${MYDEFS} md5-calc.cpp -o lab5c -lcrypto

lab5d: progress.cpp my_timestamp.cpp my_timestamp.h
	g++ ${MYDEFS} -o lab5d progress.cpp my_timestamp.cpp

clean:
	rm -f xyzzy xyzzy.log split lab5b lab5c lab5d

