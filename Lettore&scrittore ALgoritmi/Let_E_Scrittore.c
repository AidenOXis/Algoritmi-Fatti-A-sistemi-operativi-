//Problema di starving in entrambi

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

#define MUTEXL 0        // Protezione su Num_Lettori
#define MUTEXS 1        // Protezione su Num_Scrittori
#define SYNCH 2         // Mutua esclusione tra lettori e scrittori
#define MUTEXP 3        // Mutua esclusione tra scrittori

// Struttura per la memoria condivisa
struct b {
    int num_lettori;   // Numero di lettori attivi
    int num_scrittori; // Numero di scrittori attivi
    int buffer;        // Dati condivisi
};

// Operazioni di Wait e Signal per i semafori (come nel tuo esempio)
void Wait_Sem(int semid, int numsem) {
    struct sembuf sem_buf;
    sem_buf.sem_num = numsem;
    sem_buf.sem_op = -1;
    sem_buf.sem_flg = 0;
    semop(semid, &sem_buf, 1);
}

void Signal_Sem(int semid, int numsem) {
    struct sembuf sem_buf;
    sem_buf.sem_num = numsem;
    sem_buf.sem_op = 1;
    sem_buf.sem_flg = 0;
    semop(semid, &sem_buf, 1);
}

// Funzione Lettore
void lettore(struct b* buf, int semid) {
    // Inizio lettura
    Wait_Sem(semid, MUTEXL);   // Protegge il contatore dei lettori
    buf->num_lettori++;
    if (buf->num_lettori == 1) {  // Se è il primo lettore
        Wait_Sem(semid, SYNCH);   // Blocca gli scrittori
    }
    Signal_Sem(semid, MUTEXL);   // Rilascia MUTEXL

    // Lettura del buffer condiviso
    printf("Lettore: Sto leggendo il valore %d\n", buf->buffer);
    sleep(1); // Simula lettura

    // Fine lettura
    Wait_Sem(semid, MUTEXL);   // Protegge il contatore dei lettori
    buf->num_lettori--;
    if (buf->num_lettori == 0) {  // Se è l'ultimo lettore
        Signal_Sem(semid, SYNCH);  // Rilascia gli scrittori
    }
    Signal_Sem(semid, MUTEXL);   // Rilascia MUTEXL
}

// Funzione Scrittore
void scrittore(struct b* buf, int semid) {
    // Inizio scrittura
    Wait_Sem(semid, MUTEXS);       // Protegge il contatore degli scrittori
    buf->num_scrittori++;
    if (buf->num_scrittori == 1) { // Se è il primo scrittore
        Wait_Sem(semid, SYNCH);    // Blocca i lettori
    }
    Signal_Sem(semid, MUTEXS);     // Rilascia MUTEXS

    Wait_Sem(semid, MUTEXP);       // Mutua esclusione tra scrittori
    // Scrittura nel buffer condiviso
    int valore = rand() % 100;     // Genera un valore casuale
    buf->buffer = valore;
    printf("Scrittore: Ho scritto il valore %d\n", valore);
    sleep(2); // Simula il tempo di scrittura
    Signal_Sem(semid, MUTEXP);     // Rilascia MUTEXP

    // Fine scrittura
    Wait_Sem(semid, MUTEXS);       // Protegge il contatore degli scrittori
    buf->num_scrittori--;
    if (buf->num_scrittori == 0) { // Se è l'ultimo scrittore
        Signal_Sem(semid, SYNCH);  // Permette l'accesso ai lettori
    }
    Signal_Sem(semid, MUTEXS);     // Rilascia MUTEXS
}

int main() {
    // Creazione della memoria condivisa
    key_t chiaveshm = IPC_PRIVATE;
    int shmid = shmget(chiaveshm, sizeof(struct b), IPC_CREAT | 0664);
    if (shmid < 0) {
        perror("Errore creazione shm");
        exit(1);
    }

    struct b* buf = shmat(shmid, NULL, 0);
    if (buf == (void*) -1) {
        perror("Errore attach shm");
        exit(1);
    }

    // Inizializzazione della memoria condivisa
    buf->num_lettori = 0;
    buf->num_scrittori = 0;
    buf->buffer = 0; // Inizializza il buffer condiviso

    // Creazione dei semafori
    key_t chiavesem = IPC_PRIVATE;
    int semid = semget(chiavesem, 4, IPC_CREAT | 0664);
    if (semid < 0) {
        perror("Errore creazione semafori");
        exit(1);
    }

    // Inizializzazione dei semafori
    semctl(semid, MUTEXL, SETVAL, 1);   // MUTEXL per protezione su Num_Lettori
    semctl(semid, MUTEXS, SETVAL, 1);   // MUTEXS per protezione su Num_Scrittori
    semctl(semid, SYNCH, SETVAL, 1);    // SYNCH per garantire mutua esclusione
    semctl(semid, MUTEXP, SETVAL, 1);   // MUTEXP per protezione tra scrittori

    pid_t pid;

    // Creazione di due processi lettori
    for (int j = 0; j < 2; j++) {
        pid = fork();
        if (pid == 0) {
            lettore(buf, semid);
            exit(0);
        } else if (pid < 0) {
            perror("Errore creazione figlio lettore");
            exit(1);
        }
    }

    // Creazione di due processi scrittori
    for (int j = 0; j < 2; j++) {
        pid = fork();
        if (pid == 0) {
            scrittore(buf, semid);
            exit(0);
        } else if (pid < 0) {
            perror("Errore creazione figlio scrittore");
            exit(1);
        }
    }

    // Attesa della terminazione di tutti i processi figli
    for (int j = 0; j < 4; j++) {
        wait(NULL);
    }

    // Rimozione della memoria condivisa
    shmctl(shmid, IPC_RMID, 0);
    // Rimozione dei semafori
    semctl(semid, 4, IPC_RMID);
    return 0;
}
