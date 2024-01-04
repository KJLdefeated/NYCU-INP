
all:
	make -C demo all

builder:
	docker exec -ti sw_builder make -C /build clean server

test1:
	docker exec -it sw_tester /testcase/run.sh lighttpd

test2:
	docker exec -it sw_tester /testcase/run.sh demo

test3:
	docker exec -it sw_tester /testcase/run.sh demo https

clean:
	make -C demo clean
