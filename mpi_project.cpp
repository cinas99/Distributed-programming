#include "mpi.h"
#include <vector>
#include <cstdio>
#include <iostream>
#define N 150
#define S 4
#define W_KOLEJCE 0
//zadanie
#define NA_WYCIAGU 1
#define KONIEC_ZJAZDU 2
#define MSG_SIZE 4

#define RANK 0
#define ZEGAR 1
#define STAN 2
#define WAGA 3

using namespace std;

struct Narciarz {
	int zegar;
	int waga;
	int stan;
	int rank;
};

vector<Narciarz*> queue;
struct Narciarz tab[S];


void addToQueue(int *msg);
void sendMessage(int *msg, struct Narciarz *narciarz);
void receiveMessage(int rank);


void sendMessage(int *msg, struct Narciarz *narciarz){
	narciarz->zegar += 1;
	msg[RANK] = narciarz->rank;
	msg[ZEGAR] = narciarz->zegar;
	msg[STAN] = narciarz->stan;
	msg[WAGA] = narciarz->waga;
	MPI_Bcast(msg, MSG_SIZE, MPI_INT, msg[0], MPI_COMM_WORLD);
	printf("%d: Wysylam bcast\n", msg[0]);
}

void receiveMessage(int rank) {
	int msg[MSG_SIZE];
	int i;
	for(i=0; i<S; i++){
		if (i!=rank){
			MPI_Bcast(msg, MSG_SIZE, MPI_INT, i, MPI_COMM_WORLD );
			printf("%d: Otrzymalem bcast rank:%d zegar:%d stan:%d waga:%d) od %d\n", rank, msg[0], msg[1], msg[2], msg[3], i);
			addToQueue(msg);
		}
	}
}

void addToQueue(int *msg){
	//, vector<struct Narciarz*> queue){
	Narciarz *narciarz = new Narciarz();
	narciarz->rank = msg[RANK];
	narciarz->zegar = msg[ZEGAR];
	narciarz->waga = msg[WAGA];
	narciarz->stan = msg[STAN];
	queue.push_back(narciarz);
	//printf("Dopisalem do queue rank:%d zegar:%d stan:%d waga:%d) \n", msg[0], msg[1], msg[2], msg[3]);
}

void sortQueue(){
    int temp, j, r;
		for(int i=1; i<S; i++){
			temp = queue.at(i)->zegar;
			r = queue.at(i)->rank;
			j = i-1;
			while(j>=0) {
				if (queue.at(j)->zegar>temp) {
					queue.at(j+1) = queue.at(j);
				}
				else if (queue.at(j)->zegar==temp) {
					if (queue.at(j)->rank>r)
						queue.at(j+1) = queue.at(j);
				}
				j = j-1;
			}
	 		queue.at(j+1)= queue.at(i);
		}
}


int main( int argc, char **argv )
{
	int zegar = 0;
	int rank, size;
	MPI_Init( &argc, &argv );
	MPI_Comm_size( MPI_COMM_WORLD, &size );
	MPI_Comm_rank( MPI_COMM_WORLD, &rank );

	Narciarz *narciarz = new Narciarz();
	narciarz->waga = (rank*10 + 50) % N;
	narciarz->rank = rank;
	narciarz->zegar = zegar;
	narciarz->stan = W_KOLEJCE;

	int msg[MSG_SIZE];

	char processor_name[MPI_MAX_PROCESSOR_NAME];
	int namelen;

	MPI_Get_processor_name(processor_name,&namelen);
	//printf( "Jestem %d z %d na %s\n", rank, size, processor_name );
	printf( "Jestem narciarzem %d o wadze %d i stanie %d. Moj zegar to %d, nazywam sie %s\n", narciarz->rank, narciarz->waga, narciarz->stan, narciarz->zegar, processor_name);


	queue.push_back(narciarz);
	sendMessage(msg, narciarz);
	MPI_Barrier(MPI_COMM_WORLD);
	receiveMessage(rank);
	MPI_Barrier(MPI_COMM_WORLD);
	printf("%d: Skonczylem!\n", rank);
	MPI_Finalize();
}
