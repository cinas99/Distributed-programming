void sendPacket(packet_t *pkt, int destination, int tag)
{
    pthread_mutex_lock(&state_mut);
      zegarLamporta++;
      pkt->timestamp = zegarLamporta;
      MPI_Send( pkt, 1, MPI_PAKIET_T, destination, tag, MPI_COMM_WORLD);
    pthread_mutex_unlock(&state_mut);
/// end
}
