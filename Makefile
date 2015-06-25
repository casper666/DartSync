all: ./tracker/tracker ./peer/peer

#./tracker makefile
./tracker/t2p.o: ./tracker/t2p.c ./tracker/t2p.h
	gcc -pthread -Wall -lm -std=gnu99 -g -c ./tracker/t2p.c -o ./tracker/t2p.o

#./peer makefile
./peer/ptp.o: ./peer/ptp.c ./peer/ptp.h ./common/constants.h ./common/common.h
	gcc -Wall -pedantic -D_GNU_SOURCE -lm -std=gnu99 -g -pthread -c ./peer/ptp.c -o ./peer/ptp.o

./peer/resume_dl.o: ./peer/resume_dl.c ./peer/resume_dl.h ./common/constants.h ./common/common.h
	gcc -Wall -pedantic -D_GNU_SOURCE -lm -std=gnu99 -g -pthread -c ./peer/resume_dl.c -o ./peer/resume_dl.o

./peer/p2t.o: ./peer/p2t.c ./peer/p2t.h
	gcc -pthread -Wall -lm -std=gnu99 -g -c ./peer/p2t.c -o ./peer/p2t.o

#./file table makefile
./filemonitor/filetable.o: ./filemonitor/filetable.c ./filemonitor/filetable.h 
	gcc -pthread -Wall -lm -std=gnu99 -g -c ./filemonitor/filetable.c -o ./filemonitor/filetable.o 

#./peer/resume_dl.o ./peer/ptp.o
#./peer/resume_dl.o ./peer/ptp.o
	

#/. executible files
./peer/peer: ./peer/peer.c ./peer/peer.h ./peer/ptp.o ./peer/p2t.o ./peer/resume_dl.o ./filemonitor/filetable.o
	gcc ./peer/peer.c ./peer/p2t.o ./peer/ptp.o ./peer/resume_dl.o ./filemonitor/filetable.o -o ./peer/peer --std=gnu99 -lm -g -pthread -Wall
./tracker/tracker: ./tracker/tracker.h ./tracker/t2p.o ./peer/ptp.o ./filemonitor/filetable.o ./peer/resume_dl.o
	gcc ./tracker/tracker.c ./tracker/t2p.o ./peer/ptp.o ./filemonitor/filetable.o ./peer/resume_dl.o -o ./tracker/tracker --std=gnu99 -lm -g -pthread -Wall

#./ test makefiles
# ./peer/test_client_many_res: ./peer/test_client_many_res.c ./peer/ptp.o ./peer/resume_dl.o
# 	gcc -Wall -pedantic -D_GNU_SOURCE -lm -std=gnu99 -g -pthread ./peer/test_client_many_res.c ./peer/ptp.o ./peer/resume_dl.o -o ./peer/test_client_many_res
# ./peer/test_client_many_file: ./peer/test_client_many.c ./peer/ptp.o
# 	gcc -Wall -pedantic -D_GNU_SOURCE -lm -std=gnu99 -g -pthread ./peer/test_client_many_file.c ./peer/ptp.o -o ./peer/test_client_many_file
# ./peer/test_client_many: ./peer/test_client_many.c ./peer/ptp.o
# 	gcc -Wall -pedantic -D_GNU_SOURCE -lm -std=gnu99 -g -pthread ./peer/test_client_many.c ./peer/ptp.o -o ./peer/test_client_many
# ./peer/test_client: ./peer/test_client.c ./peer/ptp.o
# 	gcc -Wall -pedantic -D_GNU_SOURCE -lm -std=gnu99 -g -pthread ./peer/test_client.c ./peer/ptp.o -o ./peer/test_client
# ./peer/test_server: ./peer/test_server.c ./peer/ptp.o
# 	gcc -Wall -pedantic -D_GNU_SOURCE -lm -std=gnu99 -g -pthread ./peer/test_server.c ./peer/ptp.o -o ./peer/test_server

clean:
	rm -rf ./tracker/*.o
	rm -rf ./peer/*.o
	rm -rf ./filemonitor/*.o
	rm -f ./tracker/tracker
	rm -f ./peer/peer
	rm -f ./peer/test_client_many_res
	rm -f ./peer/test_client_many_file
	rm -f ./peer/test_client_many
	rm -f ./peer/test_client
	rm -f ./peer/test_server
	rm -f ./peer/*_tmp
