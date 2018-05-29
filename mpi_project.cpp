#include <vector>
#include "mpi.h"
#include <cstdio>
#include <ctime>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#define N 200
#define S 4 // size

// zadania

//val
#define W_KOLEJCE 0
#define NA_WYCIAGU 1
#define KONIEC_WJAZDU 2

#define MSG_SIZE 6
#define MPI_TAG 1

//typ
#define RESPONSE 4
#define REQUEST 5
#define RELEASE 6
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

	clock_mutex.lock();
	msg[ZEGAR] = zegar;
	msg[TIMESTAMP] = timestamp;
	clock_mutex.unlock();

	msg[RANK] = id;
	msg[TYP] = typ;
	msg[VAL] = val;
	msg[WEIGHT] = waga;
	MPI_Send(msg, MSG_SIZE, MPI_INT, receiver, MPI_TAG, MPI_COMM_WORLD);
}

void sendMessageToAll(int typ, int val){
	int msg[MSG_SIZE];

	clock_mutex.lock();
	msg[ZEGAR] = zegar;
	msg[TIMESTAMP] = timestamp;
	clock_mutex.unlock();

	msg[RANK] = id;
	msg[TYP] = typ;
	msg[VAL] = val;
	msg[WEIGHT] = waga;
	for(int i=0; i<size; i++){
		if (id!=i){
			MPI_Send(msg, MSG_SIZE, MPI_INT, i, MPI_TAG, MPI_COMM_WORLD);
		}
	}
}

void addToQueue(int *msg){
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
}

void sortTab(){
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
		MPI_Recv(msgRecv, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_TAG, MPI_COMM_WORLD, &status);

		if (msgRecv[TYP]==RESPONSE && msgRecv[ZEGAR]>=timestamp){
			if (msgRecv[VAL]== NA_WYCIAGU || msgRecv[VAL]==W_KOLEJCE)
				addToQueue(msgRecv);
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
		} else if (msgRecv[TYP]==RELEASE){
			addToQueue(msgRecv);
			clock_mutex.lock();
			synchClock(msgRecv[ZEGAR]);
			clock_mutex.unlock();

			lock_guard<std::mutex> lock(receiveResponses_mutex);
			processed = true;
			newMessageReceived.notify_one();
			std::cout << id << ": otrzymalem RELEASE od " << msgRecv[RANK] << '\n';
		} else {
			std::cout << "Blad!" << '\n';
		}
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
		if (queue.at(i).typ == RELEASE){
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
	clock_mutex.lock();
	if (myid<0){
		refresh();
		reqVector.push_back(narciarz);
	} else {
		reqVector.at(myid).zegar = zegar;
		reqVector.at(myid).TIMES = timestamp;
	}
	clock_mutex.unlock();
	queue.clear();
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

	int randTime = 2 + (rand() % 5);
	this_thread::sleep_for(std::chrono::seconds(randTime));
	std::cout << id << ": wjechalem na szczyt" << '\n';

	sendLiftLeft();

	int randTime1 = 5 + (rand() % 5);
	this_thread::sleep_for(std::chrono::seconds(randTime1));
	std::cout << id << ": zjechalem na nartach" << '\n';
}

void sendRequest(){
	typ = REQUEST;
	changeStatus(W_KOLEJCE);
	sendMessageToAll(typ, stan);
	refresh();
}

bool accessLift(){

	vec_mutex.lock();
	vecToTab();
	std::cout << id<< ": za vecToTab" << '\n';
	tab_mutex.lock();
	sortTab();
	std::cout << id<< ": za sortTab" << '\n';
	tab_mutex.unlock();
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

void changeStatus(int status){
	stan = status;
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
	changeStatus(KONIEC_WJAZDU);
	eraseFromVector(id);
	sendMessageToAll(RELEASE, stan);
	std::cout << id << " wysylam RELEASE do wszystkich" << '\n';
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
	threadAnswer = true;
	thread receiver (sleepAndAnswer);

	char processor_name[MPI_MAX_PROCESSOR_NAME];
	int namelen;

	stan = W_KOLEJCE;
	waga = 50 + (id*5);
	narciarz.waga = waga;
	narciarz.rank = id;
	narciarz.zegar = zegar;
	narciarz.stan = stan;
	narciarz.TIMES = timestamp;

	MPI_Get_processor_name(processor_name,&namelen);
	printf( "Jestem narciarzem %d o wadze %d i stanie %d. Moj zegar to %d, nazywam sie %s\n", narciarz.rank, narciarz.waga, narciarz.stan, narciarz.zegar, processor_name);

	while(true){

		std::cout << id << ": wysylam REQUEST do wszystkich!" << '\n';
		sendRequest();

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
		clockTimestampSynchro();
	}
delete[] tab;
MPI_Finalize();
}
