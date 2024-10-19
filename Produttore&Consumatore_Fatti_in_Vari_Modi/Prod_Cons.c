#include <sys/ipc.h> 
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

#define DIM 3 // Definizione del buffer

#define SPAZIO_LIBERO 0  // Indici
#define MSG_DISPONIBILE 1
#define MUTEXP 2 // Definisco il semaforo Mutex per il produttore 
#define MUTEXC 3 // Definisco il semaforo Mutex per il consumatore 

#define VUOTO 0  // Stati del buffer condiviso
#define IN_USO 1 
#define PIENO 2 

// Struttura del buffer condiviso 
struct b {
    int buffer[DIM];  // Buffer di dimensione DIM
    int stato[DIM];   // Array per gestire lo stato di ogni elemento del buffer 
};

void Wait_Sem(int semid, int numsem) {
    struct sembuf sem_buf;
    sem_buf.sem_num = numsem; 
    sem_buf.sem_op = -1; // Operazione di wait: decrementa il valore
    sem_buf.sem_flg = 0; // Nessun flag aggiuntivo 
    semop(semid, &sem_buf, 1); 
}

// Funzione signal 
void Signal_Sem(int semid, int numsem) {
    struct sembuf sem_buf;
    sem_buf.sem_num = numsem; 
    sem_buf.sem_op = +1; // Operazione di signal: incrementa il valore
    sem_buf.sem_flg = 0; // Nessun flag aggiuntivo 
    semop(semid, &sem_buf, 1); // Operazione sui semafori 
}

// Funzione che rappresenta il comportamento del produttore 
void produttore(struct b* buf, int semid) {
    int indice = -1;  // Inizializzazione dell'indice non valido
    Wait_Sem(semid, SPAZIO_LIBERO);
    Wait_Sem(semid, MUTEXP);
    
    // Cicla per trovare il primo elemento libero (VUOTO) nel buffer
    for (int i = 0; i < DIM; i++) {
        if (buf->stato[i] == VUOTO) {
            indice = i; // Trova un indice valido
            break;      // Esci dal ciclo appena trovi un posto libero
        }
    }

    // Verifica che sia stato trovato un indice valido
    if (indice != -1) { 
        buf->stato[indice] = IN_USO; // Segna come in uso
        Signal_Sem(semid, MUTEXP);   // Rilascia il mutex

        // Produzione del valore casuale
        int prodotto = rand() % 10;
        buf->buffer[indice] = prodotto;
        printf("Produttore: Ho prodotto %d nel buffer[%d]\n", prodotto, indice);

        buf->stato[indice] = PIENO; // Segna come pieno
        Signal_Sem(semid, MSG_DISPONIBILE); // Segnala che c'è un messaggio disponibile
    } else {
        // Se non ci sono spazi vuoti disponibili
        Signal_Sem(semid, MUTEXP); 
        printf("Produttore: Nessun spazio libero trovato\n");
    }
}

// Funzione Consumatore
void consumatore(struct b* buf, int semid) {
    int indice = -1;  // Inizializzazione dell'indice non valido
    Wait_Sem(semid, MSG_DISPONIBILE);
    Wait_Sem(semid, MUTEXC);

    // Cicla per trovare il primo elemento pieno (PIENO) nel buffer
    for (int i = 0; i < DIM; i++) {
        if (buf->stato[i] == PIENO) {
            indice = i; // Trova un indice valido
            break;      // Esci dal ciclo appena trovi un elemento pieno
        }
    }

    // Verifica che sia stato trovato un indice valido
    if (indice != -1) {
        int consumato = buf->buffer[indice]; // Consuma il valore
        buf->stato[indice] = IN_USO;         // Segna come in uso
        Signal_Sem(semid, MUTEXC);           // Rilascia il mutex

        printf("Consumatore: Ho consumato %d dal buffer[%d]\n", consumato, indice);

        buf->stato[indice] = VUOTO;          // Segna come vuoto
        Signal_Sem(semid, SPAZIO_LIBERO);    // Segnala che c'è spazio libero
    } else {
        // Se non ci sono elementi disponibili
        Signal_Sem(semid, MUTEXC); 
        printf("Consumatore: Nessun messaggio disponibile\n");
    }
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
    
    // Inizializzazione del buffer
    for (int i = 0; i < DIM; i++) {
        buf->stato[i] = VUOTO; 
    }

    // Creazione dei semafori
    key_t chiavesem = IPC_PRIVATE; 
    int semid = semget(chiavesem, 4, IPC_CREAT | 0664); 
    if (semid < 0) {
        perror("Errore creazione sem"); 
        exit(1); 
    }

    // Inizializzazione dei semafori
    semctl(semid, SPAZIO_LIBERO, SETVAL, DIM); 
    semctl(semid, MSG_DISPONIBILE, SETVAL, 0); 
    semctl(semid, MUTEXP, SETVAL, 1); 
    semctl(semid, MUTEXC, SETVAL, 1);

    pid_t pid; 
    // Creazione di due processi produttori 
    for (int j = 0; j < 2; j++) {
        pid = fork(); 
        if (pid == 0) {
            produttore(buf, semid);
            exit(0);
        } else if (pid < 0) {
            perror("Errore creazione figlio produttore"); 
            exit(1); 
        }
    }

    // Creazione di due processi consumatori
    for (int j = 0; j < 2; j++) {
        pid = fork(); 
        if (pid == 0) {
            consumatore(buf, semid);
            exit(0);
        } else if (pid < 0) {
            perror("Errore creazione figlio consumatore"); 
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

