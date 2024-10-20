//starving per gli scrittori
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

#define MUTEXL 0        // Protezione su Num_Lettori
#define SYNCH 1         // Mutua esclusione tra lettori e scrittori
#define MUTEXP 2        // Protezione per scrittori
#define MUTEXC 3        // Protezione per lettori

// Struttura per la memoria condivisa
struct b {
    int num_lettori; // Contatore lettori attivi
    int buffer;      // Dati condivisi (per esempio un messaggio)
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
    if (buf->num_lettori == 1) {
        Wait_Sem(semid, SYNCH); // Il primo lettore blocca gli scrittori
    }
    Signal_Sem(semid, MUTEXL); // Rilascia il semaforo dei lettori

    // Lettura del buffer condiviso
    printf("Lettore: Sto leggendo il valore %d\n", buf->buffer);
    sleep(1); // Simula il tempo di lettura

    // Fine lettura
    Wait_Sem(semid, MUTEXL);   // Protegge il contatore dei lettori
    buf->num_lettori--;
    if (buf->num_lettori == 0) {
        Signal_Sem(semid, SYNCH); // L'ultimo lettore sblocca gli scrittori
    }
    Signal_Sem(semid, MUTEXL); // Rilascia il semaforo dei lettori
}

// Funzione Scrittore
void scrittore(struct b* buf, int semid) {
    // Inizio scrittura
    Wait_Sem(semid, SYNCH);    // Scrittore attende che nessun lettore sia attivo

    // Scrittura nel buffer condiviso
    int valore = rand() % 100; // Genera un valore casuale
    buf->buffer = valore;
    printf("Scrittore: Ho scritto il valore %d\n", valore);
    sleep(2); // Simula il tempo di scrittura

    // Fine scrittura
    Signal_Sem(semid, SYNCH);  // Rilascia l'accesso per altri lettori/scrittori
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
    buf->buffer = 0; // Inizializza il buffer condiviso

    // Creazione dei semafori
    key_t chiavesem = IPC_PRIVATE;
    int semid = semget(chiavesem, 4, IPC_CREAT | 0664);
    if (semid < 0) {
        perror("Errore creazione semafori");
        exit(1);
    }

    // Inizializzazione dei semafori
    semctl(semid, MUTEXL, SETVAL, 1);   // MUTEXL per i lettori
    semctl(semid, SYNCH, SETVAL, 1);    // SYNCH per lettori/scrittori
    semctl(semid, MUTEXP, SETVAL, 1);   // MUTEXP (se necessario per estensioni)
    semctl(semid, MUTEXC, SETVAL, 1);   // MUTEXC (se necessario per estensioni)

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
