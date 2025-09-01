MYDEFS = -g -Wall -std=c++11 -DLOCALHOST=\"127.0.0.1\"

clean:
	rm -f a.out

# Lab 6

first: first.cpp
	g++ ${MYDEFS} first.cpp -pthread

first-arg: first-arg.cpp
	g++ ${MYDEFS} first-arg.cpp -pthread

fac: fac.cpp
	g++ ${MYDEFS} fac.cpp -pthread

create: create.cpp
	g++ ${MYDEFS} create.cpp -pthread

bad-create: bad-create.cpp
	g++ ${MYDEFS} bad-create.cpp -pthread

exit-detach: exit-detach.cpp
	g++ ${MYDEFS} exit-detach.cpp -pthread

detach: detach.cpp
	g++ ${MYDEFS} detach.cpp -pthread

join: join.cpp
	g++ ${MYDEFS} join.cpp -pthread

lambda-join: lambda-join.cpp
	g++ ${MYDEFS} lambda-join.cpp -pthread

lambda-join-ref: lambda-join-ref.cpp
	g++ ${MYDEFS} lambda-join-ref.cpp -pthread

bad-dyn-create: bad-dyn-create.cpp
	g++ ${MYDEFS} bad-dyn-create.cpp -pthread

bad-dyn-create2: bad-dyn-create2.cpp
	g++ ${MYDEFS} bad-dyn-create2.cpp -pthread

bad-dyn-create3: bad-dyn-create3.cpp
	g++ ${MYDEFS} bad-dyn-create3.cpp -pthread

bad-dyn-create4: bad-dyn-create4.cpp
	g++ ${MYDEFS} bad-dyn-create4.cpp -pthread

join-dyn: join-dyn.cpp
	g++ ${MYDEFS} join-dyn.cpp -pthread

join-dyn4: join-dyn4.cpp
	g++ ${MYDEFS} join-dyn4.cpp -pthread

join-shared: join-shared.cpp
	g++ ${MYDEFS} join-shared.cpp -pthread

join-new: join-new.cpp
	g++ ${MYDEFS} join-new.cpp -pthread

# Lab 7

race: race.cpp
	g++ ${MYDEFS} race.cpp -pthread

no-race: no-race.cpp
	g++ ${MYDEFS} no-race.cpp -pthread

no-race-atomic: no-race-atomic.cpp
	g++ ${MYDEFS} no-race-atomic.cpp -pthread

# Lab 9

cv: cv.cpp
	g++ ${MYDEFS} cv.cpp -pthread

