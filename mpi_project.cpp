#include <vector>
#include "mpi.h"
#include <cstdio>
#include <ctime>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#define N 150
#define S 4 // size

// zadania

//val
#define W_KOLEJCE 0
#define NA_WYCIAGU 1
#define KONIEC_WJAZDU 2

#define MSG_SIZE 6
#define MPI_TAG 1

//typ
#define INFO 0
#define POTWIERDZENIE 1
#define STAN 2
#define WAGA 3
#define RESPONSE 4
#define REQUEST 5
#define END 6
#define WEIGHT 5

#define RANK 0
#define ZEGAR 1
#define TYP 2
#define VAL 3
#define TIMESTAMP 4

using namespace std;

struct Narciarz {
	int rank;
	int zegar;
	int stan;
	int waga;
	int TIMES;
};

struct Package {
	int rank;
	int zegar;
	int typ;
	int val;
	int TIMES;
	int weight;
};

int zegar = 0;
int timestamp = 0;
int stan = 0;
int typ = 0;
int waga = 0;
int permissions = 0;
int procInQueue = 0;
int id;
int size = S;
bool threadAnswer = false;
bool processed = false;
bool ready = false;

mutex queue_mutex, clock_mutex, stan_mutex, receiveResponses_mutex, m, tab_mutex, vec_mutex;
condition_variable newMessageReceived, cv;

vector<Package> queue;
vector<Narciarz> reqVector;
Narciarz *tab;
Narciarz narciarz = Narciarz();


void addToQueue(int *msg);
void sendMessageToAll();
void receiveMessage();
bool canEnterIntoLift();
void incrementLamportClock();
void refresh();
void synchClock(int recivedClock);
void sendLiftLeft();
void changeStatus(int stan);

void sendMessage(int receiver, int typ, int val){
	int msg[MSG_SIZE];
	//incrementLamportClock();

	clock_mutex.lock();
	msg[ZEGAR] = zegar;
	msg[TIMESTAMP] = timestamp;
	clock_mutex.unlock();

	msg[RANK] = id;
	msg[TYP] = typ;
	msg[VAL] = val;
	msg[WEIGHT] = waga;
	//MPI_Bcast(msg, MSG_SIZE, MPI_INT, msg[0], MPI_COMM_WORLD);
	MPI_Send(msg, MSG_SIZE, MPI_INT, receiver, MPI_TAG, MPI_COMM_WORLD);
	//cout << id << ": " << "wyslalem do " << receiver << "\n";
	//printf("%d: Wysylam bcast\n", msg[0]);
}

void sendMessageToAll(int typ, int val){
	int msg[MSG_SIZE];

	clock_mutex.lock();
	//incrementLamportClock();
	msg[ZEGAR] = zegar;
	msg[TIMESTAMP] = timestamp;
	clock_mutex.unlock();

	msg[RANK] = id;
	msg[TYP] = typ;
	msg[VAL] = val;
	msg[WEIGHT] = waga;
	//MPI_Bcast(msg, MSG_SIZE, MPI_INT, msg[0], MPI_COMM_WORLD);
	for(int i=0; i<size; i++){
		if (id!=i){
			MPI_Send(msg, MSG_SIZE, MPI_INT, i, MPI_TAG, MPI_COMM_WORLD);
			//cout<< narciarz.rank<<": " << "wyslalem do "<<i<<"\n";
		}
	}
	//printf("%d: Wysylam bcast\n", msg[0]);
}

void receiveMessage() {
	int msg[MSG_SIZE];
	int i;
	MPI_Status status;
	for(i=0; i<size; i++){
		if (i!=id){
			MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			//MPI_Bcast(msg, MSG_SIZE, MPI_INT, i, MPI_COMM_WORLD);
			//printf("%d: Otrzymalem bcast id:%d zegar:%d stan:%d waga:%d) od %d\n", id, msg[0], msg[1], msg[2], msg[3], i);
			addToQueue(msg);
		}
	}
}

void sendConfirm(int receiver){
	int msg[MSG_SIZE];
	msg[RANK] = id;
	msg[TYP] = INFO;
	msg[VAL] = POTWIERDZENIE;
	msg[ZEGAR] = zegar;
	MPI_Send(msg, MSG_SIZE, MPI_INT, receiver, MPI_TAG, MPI_COMM_WORLD);
	std::cout << id << ": wysylam potwierdzenie" << '\n';
}

void addToQueue(int *msg){
	//, vector<struct Narciarz*> queue){
	//Narciarz n = Narciarz();
	Package n = Package();
	n.rank = msg[RANK];
	n.zegar = msg[ZEGAR];
	n.typ = msg[TYP];
	n.val = msg[VAL];
	n.TIMES = msg[TIMESTAMP];
	n.weight = msg[WEIGHT];
	queue_mutex.lock();
	queue.push_back(n);
	queue_mutex.unlock();
	//printf("Dopisalem do queue rank:%d zegar:%d stan:%d waga:%d) \n", msg[0], msg[1], msg[2], msg[3]);
}

void initTab(){
	for(int n=0; n<queue.size(); n++){
		Narciarz adam = Narciarz();
		adam.rank = queue.at(n).rank;
		adam.zegar = queue.at(n).zegar;
		adam.TIMES = queue.at(n).TIMES;
		int type = queue.at(n).typ;
		if (type == WAGA){
			adam.waga = queue.at(n).val;
		}
		tab[n] = adam;
	}
}

void vecToTab(){
	delete[] tab;
	procInQueue = reqVector.size();
	tab = new Narciarz[procInQueue];
	for(int n=0; n<procInQueue; n++)
		tab[n] = reqVector.at(n);
}

void tabToVec(){
	reqVector.clear();
	for (int i = 0; i<procInQueue; i++) {
		reqVector.push_back(tab[i]);
	}
	/*
	for(int i = 0; i<size; i++){
		Narciarz adam = Narciarz();
		adam.rank = tab[i].rank;
		adam.zegar = tab[i].zegar;
		adam.TIMES = tab[i].TIMES;
		adam.waga = tab[i].weight;
		reqVector.push_back(adam);
	}
	*/
}

void sortTab(){
	//for(int n=0; n<size; n++)
	//tab[n] = queue.at(n);
	int j;
	Narciarz temp;
	for(int i=1; i<procInQueue; i++){
		temp = tab[i];
		j = i-1;
		while(j>=0 && (tab[j].TIMES>temp.TIMES || (tab[j].TIMES==temp.TIMES && tab[j].rank>temp.rank))) {
			tab[j+1] = tab[j];
			j = j-1;
		}
		tab[j+1] = temp;
	}
}

void printVec(){
	for(int i=0; i<reqVector.size(); i++){
		printf("%d: kolejka rank:%d zegar:%d timestamp:%d waga:%d\n", id, reqVector.at(i).rank, reqVector.at(i).zegar, reqVector.at(i).TIMES, reqVector.at(i).waga);
	}
}

void printTab(){
	for(int i=0; i<size; i++){
		printf("%d: kolejka rank:%d timestamp:%d waga:%d\n", id, tab[i].rank, tab[i].TIMES, tab[i].waga);
	}
}



void eraseFromVector(int i){
	for (int j = 0; j<reqVector.size(); j++){
		if (i==reqVector.at(j).rank)
			reqVector.erase(reqVector.begin()+j);
	}
}


void sleepAndAnswer(){
	MPI_Status status;
	int msgRecv[MSG_SIZE];
	int msgSend[MSG_SIZE];


	while(threadAnswer) {
		MPI_Recv(msgRecv, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

		if (msgRecv[TYP]==RESPONSE && msgRecv[ZEGAR]>=timestamp){
			if (msgRecv[VAL]== NA_WYCIAGU || msgRecv[VAL]==W_KOLEJCE){
				addToQueue(msgRecv);
				//procInQueue++;
			}
			permissions++;

			clock_mutex.lock();
			synchClock(msgRecv[ZEGAR]);
			clock_mutex.unlock();

			std::cout << id << ": dostalem RESPONSE od " << msgRecv[RANK] << '\n';
			if (permissions==size-1){
				lock_guard<std::mutex> lk(m);
				ready = true;
				cv.notify_one();
				permissions = 0;
				std::cout << id << ": otrzymalem wszytkie zgody!" << '\n';
			}
		} else if (msgRecv[TYP]==REQUEST) {
			addToQueue(msgRecv);

			clock_mutex.lock();
			synchClock(msgRecv[ZEGAR]);
			msgSend[ZEGAR] = zegar;
			msgSend[TIMESTAMP] = timestamp;
			clock_mutex.unlock();

			msgSend[RANK] = id;
			msgSend[TYP] = RESPONSE;
			msgSend[VAL] = stan;
			MPI_Send(msgSend, MSG_SIZE, MPI_INT, msgRecv[RANK], MPI_TAG, MPI_COMM_WORLD);
			std::cout << id << ": otrzymalem REQUEST od " << msgRecv[RANK] << '\n';
		} else if (msgRecv[TYP]==END){
			addToQueue(msgRecv);
			clock_mutex.lock();
			synchClock(msgRecv[ZEGAR]);
			clock_mutex.unlock();

			//addToQueue(msgRecv);
			//vec_mutex.lock();
			//eraseFromVector();
			//vec_mutex.unlock();

			lock_guard<std::mutex> lock(receiveResponses_mutex);
			processed = true;
			newMessageReceived.notify_one();
			std::cout << id << ": otrzymalem END od " << msgRecv[RANK] << '\n';
		}
		/*
		//gdy zadanie to odpowiadamy zgoda lub dodajemy do kolejki oczekujacych
		if (msgRecv[TYP] == STAN && msgRecv[VAL] = W_KOLEJCE){
		if (msgRecv[ZEGAR] < zegar || (msgRecv[ZEGAR]==zegar && msgRecv[RANK]<id)){
		sendMessage(msgRecv[RANK], INFO, POTWIERDZENIE);
	} else {
	lock_guard<std::mutex> lk(receiveResponses_mutex);
	addToQueue(msgRecv);
	processed = true;
	cout<< id <<": otrzymalem wiadomosc od " << msgRecv[RANK] << "\n";
	//lk.unlock();
	newMessageReceived.notify_one();
}
} else if (msgRecv[TYP] == INFO && msgRecv[VAL] == POTWIERDZENIE) {

}
*/
}
}


int findProcess(int i){
	for(int j = 0; j<size; j++){
		if (i==tab[j].rank) {
			return j;
		}
	}
	return -1;
}

int findInVector(int i){
	for (int j=0; j<reqVector.size(); j++){
		if (i==reqVector.at(j).rank)
			return j;
	}
	return -1;
}

void handler(){

	for (int i = 0; i<queue.size(); i++){
		if (queue.at(i).typ == END){
			eraseFromVector(queue.at(i).rank);
		} else if (queue.at(i).typ == REQUEST || queue.at(i).typ == RESPONSE){
			int pid = findInVector(queue.at(i).rank);
			if (pid<0){
				Narciarz adam = Narciarz();
				adam.rank = queue.at(i).rank;
				adam.zegar = queue.at(i).zegar;
				adam.TIMES = queue.at(i).TIMES;
				adam.waga = queue.at(i).weight;
				reqVector.push_back(adam);
			} else {
				reqVector.at(pid).zegar = queue.at(i).zegar;
				reqVector.at(pid).TIMES = queue.at(i).TIMES;
			}
		}
	}
	int myid = findInVector(id);
	if (myid<0){
		refresh();
		reqVector.push_back(narciarz);
	} else {
		reqVector.at(myid).zegar = zegar;
		reqVector.at(myid).TIMES = timestamp;
	}
	queue.clear();

/*
	for (int i=0; i<queue.size(); i++){
		int id = queue.at(i).rank;
		int j = queue.size() - 1;
			while(j!=i){
				if (queue.at(j).rank==id) break;
				j--;
			}
	}
	*/
}

void synchClock(int recivedClock){
	if(zegar < recivedClock)
	zegar = recivedClock;
	zegar++;
}

void intoLift(){
	stan_mutex.lock();
	changeStatus(NA_WYCIAGU);
	stan_mutex.unlock();

	/*
	clock_mutex.lock();
	incrementLamportClock();
	clock_mutex.unlock();
	*/
	//thread receiver (sleepAndAnswer);
	int randTime = 2 + (rand() % 5);
	this_thread::sleep_for(std::chrono::seconds(randTime));
	std::cout << id << ": wjechalem na szczyt" << '\n';

	sendLiftLeft();

	int randTime1 = 5 + (rand() % 5);
	this_thread::sleep_for(std::chrono::seconds(randTime1));
	std::cout << id << ": zjechalem na nartach" << '\n';
	//threadAnswer = false;
}

void sendRequest(){
	typ = REQUEST;
	sendMessageToAll(typ, REQUEST);
	refresh();
}

bool accessLift(){

	vec_mutex.lock();
	vecToTab();
	std::cout << id<< ": za vecToTab" << '\n';
	//queue_mutex.lock();
	tab_mutex.lock();
	sortTab();
	std::cout << id<< ": za sortTab" << '\n';
	tab_mutex.unlock();
	//queue_mutex.unlock();
	tabToVec();
	std::cout << id<< ": za tabToVec" << '\n';
	vec_mutex.unlock();

	//warunek na wejscie na wyciagu
	if (canEnterIntoLift()) {
		std::cout << id << ": wchodze na wyciag!" << '\n';
		return true;
	} else {
		std::cout << id << ": nie moge wejsc na wyciag!" << '\n';
		return false;
	}
}

bool canEnterIntoLift(){
	int i = 0;
	int sum = 0;
	int pid;

	//dodaj wagi narciarzy stojacych przede mna w kolejce
	do {
		pid = reqVector.at(i).rank;
		sum += reqVector.at(i).waga;
		i++;
	} while (i<size && pid!=id);
	std::cout << '\n';
	printVec();
	std::cout << '\n';

	std::cout << id << ": suma wag narciarzy przede mna: " << sum << '\n';
	if (sum>N)
	return false;
	else return true;
}

void incrementLamportClock(){
	zegar++;
	timestamp++;
}

void changeStatus(int stan){
	narciarz.stan = stan;
}

void refresh(){
	narciarz.zegar = zegar;
	narciarz.stan = stan;
	narciarz.TIMES = timestamp;
}

void clockTimestampSynchro() {
	clock_mutex.lock();
	incrementLamportClock();
	timestamp = zegar;
	clock_mutex.unlock();
}

void sendLiftLeft(){

	clockTimestampSynchro();

	stan = KONIEC_WJAZDU;
	//int j = findProcess(id);
	//if (j!=-1 && j<size-1)
	//	for (j = j + 1; j < size; j++)
	//sendMessage(tab[j].rank, END, stan);
	eraseFromVector(id);
	sendMessageToAll(END, stan);
	std::cout << id << " wysylam END do wszystkich" << '\n';
	//refresh();
}

bool waitForPlace(){
	queue_mutex.lock();
	tab_mutex.lock();
	handler();
	tab_mutex.unlock();
	queue_mutex.unlock();
	std::cout << id << ": Handler udany!" << '\n';
	return accessLift();
}

int main( int argc, char **argv )
{
	srand(time(NULL));
	MPI_Init( &argc, &argv );
	MPI_Comm_size( MPI_COMM_WORLD, &size );
	MPI_Comm_rank( MPI_COMM_WORLD, &id );

	int msg[MSG_SIZE];
	tab = new Narciarz[size];

	char processor_name[MPI_MAX_PROCESSOR_NAME];
	int namelen;

	stan = W_KOLEJCE;
	waga = (id*10 + 50) % N;
	narciarz.waga = waga;
	narciarz.rank = id;
	narciarz.zegar = zegar;
	narciarz.stan = stan;
	narciarz.TIMES = timestamp;

	MPI_Get_processor_name(processor_name,&namelen);
	//printf( "Jestem %d z %d na %s\n", rank, size, processor_name );
	printf( "Jestem narciarzem %d o wadze %d i stanie %d. Moj zegar to %d, nazywam sie %s\n", narciarz.rank, narciarz.waga, narciarz.stan, narciarz.zegar, processor_name);

	// inicjalizacja tablic
	/*
	sendMessageToAll(WAGA, narciarz.waga);
	refresh();
	Package me = Package();
	me.rank = id;
	me.zegar = zegar;
	me.typ = WAGA;
	me.val = narciarz.waga;
	me.TIMES = timestamp;
	me.weight = waga;
	queue.push_back(me);
	//tab[size-1] = narciarz;


	receiveMessage();
	initTab();
	queue.clear();
	sortTab();
	//printTab();

	//tabToVec();
	//printVec();
	std::cout << "\n" << '\n';

	MPI_Barrier(MPI_COMM_WORLD);
	*/

	//petla while
	threadAnswer = true;
	thread receiver (sleepAndAnswer);

	while(true){
		//threadAnswer = true;
		//wyslij zadanie
		//clock_mutex.lock();
		clockTimestampSynchro();

		sendRequest();
		//clock_mutex.unlock();
		std::cout << id << ": wyslalem REQUEST do wszystkich!" << '\n';
		std::unique_lock<std::mutex> lk(m);
		cv.wait(lk, []{return ready;});
		std::cout << id << ": jestem za cv.wait" << '\n';
		ready = false;
//aktualizacja czasow w tablicy
		queue_mutex.lock();
		tab_mutex.lock();
		handler();
		tab_mutex.unlock();
		queue_mutex.unlock();


		std::cout << id << ": uaktualnilem tablice" << '\n';
		if (accessLift()){
			intoLift();
			/*
			stan_mutex.lock();
			changeStatus(NA_WYCIAGU);
			stan_mutex.unlock();

			clock_mutex.lock();
			incrementLamportClock();
			clock_mutex.unlock();

			//thread receiver (sleepAndAnswer);
			int randTime = 1000 + (rand() % 2000);
			this_thread::sleep_for(std::chrono::milliseconds(randTime));
			std::cout << id << ": wjechalem na szczyt" << '\n';
			sendLiftLeft();

			int randTime1 = 1000 + (rand() % 2000);
			this_thread::sleep_for(std::chrono::milliseconds(randTime1));
			std::cout << id << ": zjechalem na nartach" << '\n';
			threadAnswer = false;
			//receiver.join();
			*/
		} else {
			do {
				unique_lock<std::mutex> lock(receiveResponses_mutex);
				std::cout << id << ": czekam na zwolnienie miejsca" << '\n';
				newMessageReceived.wait(lock, []{return processed;});
				std::cout << id << ": Wybudzilem sie!" << '\n';
				processed = false;
			} while (!waitForPlace());
			intoLift();
		}
	}
delete[] tab;
MPI_Finalize();

}
