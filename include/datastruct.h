/*
    Autore: Fabrizio Cau
    Matricola: 508700
    Corso A Informatica 
 
    Header libreria datastruct (gestore struttura dati)
*/

#include <pthread.h>
#include <mylib.h>
#include <signal.h>

// Variabili di stato dello store usate in condivisione tra i thread
volatile sig_atomic_t entranteStatus;
volatile sig_atomic_t fastExit;
volatile sig_atomic_t checkoutThreadStatus;
volatile sig_atomic_t custThreadStatus;

// Struttura dati dei clienti (slot) all'interno dello store
typedef struct cliente {
    int idCli; // Id cliente
    int CurCkout; // Numero cassa attuale
    int pos; // Posizione in coda 
    int status; // Stato cliente: [0]: slot vuoto (cliente uscito), 1: slot pieno (cliente è entrato), 2: cliente è in cassa, 3: il cliente viene servito
    unsigned int nProd; // Numero di prodotti "nel carrello"
    unsigned int TotCode; // Numero di code visitate
    unsigned int ShopTime; // Tempo dedicato agli acquisti in ms
    unsigned long CheckoutTime; // Tempo passato in coda alle casse in ms
    pthread_t tidCliente; 
    pthread_mutex_t mtx;
    pthread_cond_t cond;
} cliente_t;

typedef struct node { // Per linked list di Clienti
    cliente_t *Cliente;
    struct node *next;
} node_t;

typedef struct timesNode { // Per linked list di intervalli di tempi
    unsigned long t;
    struct timesNode *next;
} timesNode_t;

// Struttura dati delle casse
typedef struct cassa {
    int Ncassa; // Numero cassa
    int inCoda; // Numero clienti in coda
    int TotServiti; // Clienti serviti in totale
    int TotProd; // Prodotti passati in totale 
    int ClosingCount; // Numero di chiusure
    short isOpen; // Indicatore di cassa aperta/chiusa
    unsigned int baseServiceTime; // Tempo di servizio base
    pthread_t tidCassa;
    timesNode_t *stHead; // Primo elemento della linked list di tempi di servizio
    timesNode_t *stTail; // Ultimo elemento della linked list di tempi di servizio
    timesNode_t *openTimeHead; // Primo elemento della linked list dei tempi di apertura
    timesNode_t *openTimeTail; // Ultimo elemento della linked list dei tempi di apertura
    ts_t tmp; // Variabile temporanea per registrare "l'orario" dell'ultima apertura
    node_t *Head; // Primo elemento della linked list di clienti
    node_t *Tail; // Ultimo elemento della linked list di clienti
    pthread_mutex_t mtx;
    pthread_cond_t cond;
} cassa_t;

cassa_t *Casse;
cassa_t *npe;

// Struttura dati statistiche clienti
pthread_mutex_t StatsMtx = PTHREAD_MUTEX_INITIALIZER;
int CustCount; // numero clienti totali (incrementale)
int ProdCount; // numero prodotti totali acqusitati
node_t *CustStatsHead, *CustStatsTail; // Primo e ultimo elemento della linked list di clienti
   
// Clienti in coda all'ingresso
pthread_mutex_t wcMtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wcCond = PTHREAD_COND_INITIALIZER; // var cond per i thread clienti
pthread_cond_t genCond = PTHREAD_COND_INITIALIZER; // var cond per i produttore
int *waitingCustomers; // Array di Id dei clienti fuori
int waitingLen; // Numero di clienti in attesa fuori
int current; // Numero clienti dentro

// ===== INIZIALIZZAZIONE STRUTTURE DATI =====
void initCasse(); // Casse
void initNPE(); // Uscita senza acqusiti (no purchase exit)
cliente_t* initClienti(); // Clienti
void initCustStats(); // Statistiche clienti
int* initWaitingCustomers(); // Nuovi clienti (in attesa fuori)
// ===========================================

// ===== PUSH & POP ==========================
void PushCustStats(cliente_t *c); // Push cliente c nella lista delle statistiche
void pushCliente(cliente_t *Cli, int nc); // Push cliente Cli in coda alla cassa nc
int extractCust(cliente_t *Cli, int nc); // Estrazione del cliente Cli dalla cassa nc)
void popCliente(cassa_t *c); // Pop del primo cliente in cassa c
// ===========================================

// ==== APERTURA E CHIUSURA CASSE ============
void apriCassa(int n); // Se non sono tutte aperte: apre la cassa n se esiste ed è chiusa, altrimenti una random
void chiudiCassa(int n); // Se vi sono almeno 2 casse aperte (o lo store sta chiudendo) chiude la cassa n se esiste ed è aperta, altrimenti una random
// ===========================================

// ===== FUNZIONI SUPPORTO ===================
int* listCasse(int isOp); // Restituisce una lista di casse (isOp: 1 aperte, 0 chiuse), in cui il primo elemento ne indica la quantità. 
short stillOpen(int nc); // Restituisce 1 se la cassa nc è aperta, 0 altrimenti
void updatePositions(node_t *h); // Aggiorna la posizione in coda dei clienti da h in poi
node_t* RecFreeNodeStats(node_t *n); // Dealloca ricorsivamente la coda (e i clienti in esso) puntata da n
node_t* RecFreeNodeOnly(node_t *n); // Dealloca ricorsivamente la coda (non i clienti) puntata da n
timesNode_t* RecFreeTimesNode(timesNode_t *ts); // Dealloca la linked list di intervalli puntata da ts
int bestCheckout(int pos, int curCk); // Restituisce il numero della cassa con meno clienti rispetto alla posizione pos in cassa curCk
// ===========================================


// ===== STAMPA DEL FILE LOG =================
void PrintLog();
// ===========================================

// ===== MUTEX & VAR COND ====================
void Lock(pthread_mutex_t *m);
void Unlock(pthread_mutex_t *m);
void Signal(pthread_cond_t *c);
void Broadcast(pthread_cond_t *c);
void Wait(pthread_cond_t *c, pthread_mutex_t *m);
void InitMCCassa(cassa_t *c); 
void InitMCCli(cliente_t *c);
// ===========================================
