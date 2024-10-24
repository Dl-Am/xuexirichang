# test:test.c ThreadPool.c
# 	gcc -o $@ $^ -lpthread

# .PHONY:clean
# clean:
# 	rm -r test

test:TaskQueue.cpp ThreadPool.cpp Main.cc
	g++ -o $@ $^ -lpthread -std=c++11 -g

.PHONY:clean
clean:
	rm -r test