gobang:gobang.cc logger.hpp util.hpp db.hpp online.hpp room.hpp 
	g++ -g -o$@ $^ -L/usr/lib64/mysql -lmysqlclient -ljsoncpp -lpthread -lboost_system
.PHONY:
clean:
	rm -f gobang