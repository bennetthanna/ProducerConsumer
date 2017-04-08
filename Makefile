all: producer_consumer.cpp
	g++ -pthread -Wall -o a.out producer_consumer.cpp

clean:
	$(RM) a.out