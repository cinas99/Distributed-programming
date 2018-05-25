#include "mpi.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#define N 100
#define S 4
#define W_KOLEJCE 0
//zadanie
#define NA_WYCIAGU 1
#define KONIEC_ZJAZDU 2
#define MSG_SIZE 3


struct Narciarz {
	int zegar;
	int waga;
	int stan;
	int rank;
};
/*
struct Narciarz dodajDoTablicy(struct Narciarz *head, int){
	struct Narciarz *kolejka = (Narciarz*)malloc(sizeof(Narciarz));
	if(kolejka == NULL){
        fprintf(stderr, "Unable to allocate memory\n");
        exit(-1);
    }
};
*/



int main( int argc, char **argv )
{
	int zegar = 0;
	int rank, size;
	MPI_Init( &argc, &argv );
	MPI_Comm_size( MPI_COMM_WORLD, &size );
	MPI_Comm_rank( MPI_COMM_WORLD, &rank );
	//struct Queue kolejka[size];
	
	
	struct Narciarz *narciarz = (struct Narciarz*)malloc(sizeof(struct Narciarz));
	narciarz->waga = (rank*10 + 50) % N;
	narciarz->rank = rank;
	narciarz->zegar = zegar;
	narciarz->stan = W_KOLEJCE;
	struct Narciarz queue[size];
	queue[rank] = *narciarz;

	int msg[4];
	

	char processor_name[MPI_MAX_PROCESSOR_NAME];
	int namelen;
	
	MPI_Get_processor_name(processor_name,&namelen);
	//printf( "Jestem %d z %d na %s\n", rank, size, processor_name );
	printf( "Jestem narciarzem %d o wadze %d i stanie %d. Moj zegar to %d, nazywam sie %s\n", narciarz->rank, narciarz->waga, narciarz->stan, narciarz->zegar, processor_name);

	int i = 0;
	for (i;i<size;i++){
		if (rank == i) {
			msg[0] = narciarz->rank;
			msg[1] = narciarz->zegar + 1;
			msg[2] = narciarz->stan;
			msg[3] = narciarz->waga;
			MPI_Bcast(msg, 4, MPI_INT, i, MPI_COMM_WORLD);
			printf("%d: Wysyłam bcast\n", rank);
		}
		else {
			MPI_Bcast( msg, 4, MPI_INT, i, MPI_COMM_WORLD );
			printf("%d: Otrzymalem bcast (%d %d %d %d) od %d\n", rank, msg[0], msg[1], msg[2], msg[3], i);
		}
	}


	MPI_Finalize();
}
