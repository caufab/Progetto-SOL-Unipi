/*
    Autore: Fabrizio Cau
    Matricola: 508700
    Corso A Informatica 
 
    Header libreria mylib (funzioni supporto)
*/

#define _POSIX_C_SOURCE 200112L

// valori default 
#define def_LOGFILE "logsupermercato.txt"
#define def_FORK_FROM_MNG 0
#define def_K 10 // numero massimo di casse
#define def_T 200 // tempo massimo di permanenza del cliente in ms 
#define def_P 15 // numero massimo di prodotti per cliente
#define def_C 20 // numero massimo di clienti dentro il supermercato
#define def_E 1 // nel supermercato possono entrare nuovi clienti quando ce ne sono al più C-E
#define def_TIME_PER_PROD 600 // tempo di servizio per prodotto in ms
#define def_MAX_BASE_ST 802 // tempo di servizio base max
#define def_INCODA_NOTIFY_TIMEOUT 4000 // intervallo di comunicazione del valore
#define def_CUST_MOVES_TIMEOUT 2500 // intervallo di controllo code del cliente
#define def_INITIAL_CHECKOUTS 2 // casse aperte inizialmente
#define def_MAX_EMPTY_CKOUTS 2 // Soglia S1: massimo numero di casse aperte con inCoda
#define def_MAX_CUST_PER_CKOUT 10 // Soglia S2: lunghezza coda minima per aprire una cassa in più

char LOGFILE[100];
int FORK_FROM_MNG;
int K;
int T;
int P;
int C;
int E;
int TIME_PER_PROD;
int MAX_BASE_ST;
int INCODA_NOTIFY_TIMEOUT;
int CUST_MOVES_TIMEOUT;
int INITIAL_CHECKOUTS;
int MAX_EMPTY_CKOUTS;
int MAX_CUST_PER_CKOUT;

// valori default del sistema 
#define MAXSLEEP 100000
#define CONFIGPATH "config.txt"
#define SOCKETPATH "./socket"
#define UNIX_PATH_MAX 108


// macro controllo valori
#define SP_ER(a) if((a)<0) { fprintf(stderr, "sscanf/sprintf error\n"); exit(-1); } // sscanf/sprintf error
#define FKE(a) if((a)==-1) { perror("fork"); exit(errno); } //fork error
#define TE(a) if((a)!=0) { perror("thread"); exit(errno); } //thread error
#define MCE(a,str) if((a)!=0) { perror(str); exit(errno); } // mutex/cond error
#define EQ(a,b) if((a)!=(b)) { fprintf(stderr, "NOT EQUAL\n"); exit(-1); } // not equal error
#define ARG_ERR(s) { fprintf(stderr, "%s: invalid argument\n",s); exit(-1); } // invalid argument error
#define MALLOC_ERR(str) { perror(str); exit(errno); } // malloc error
#define FE(a) if((a)==NULL) { perror("fopen"); return; } // file error
#define Exit_ERR(str) { fprintf(stderr, "%s", str); exit(-1); } // generic error with exit
#define RWE(a) while((a)==-1) { if(errno!=EINTR) { perror("read/write"); exit(errno); } }  // read/write socket error
#define CONN_ER(a) while((a)==-1) { if(errno!=ENOENT) { perror("connect"); exit(errno); } else { sleepms(500); } } // connect call error
#define SKT_ER(a) if((a)==-1) { perror("socket"); exit(errno); } // socket error
#define SIG_ER(str, a) if((a)!=0) { fprintf(stderr, "%s error", str); exit(-1); } // sigaction call error
#define CLK_ER(a) if((a)!=0) { fprintf(stderr, "clock_gettime error: %d", errno); exit(-1); }

typedef struct timespec ts_t; 

// Descrittori di file per la comunicazione tramite socket
int Skt_FD, SktCon_FD;

// Funzioni di supporto:

// Assegna i parametri di configurazione di default
void SetDefConf();

// Assegna il valore val alla variabile name, se esiste, tra i parametri di configurazione
void setConfValue(char *name, char *val);

// Esegue il parsinig del file config
void ParseConfig();

// Manda le coppie nomi, valori degli argv a setConfValue
void OverwriteConfig(int n, char *el[]);

// Mette il thread in attesa di ms millisecondi
void sleepms(unsigned int ms);

// Restituisce 1 se n è contenuto in list, 0 altrimenti
int Exists(int *list, int n);

// Restituisce un intero random dalla lista list
int getRandom(int *list);

// Restituisce l'intervallo tra start e stop in millisecondi
unsigned long GetTimeDiff(ts_t start, ts_t stop);

// Pulizia del file socket
void CleanSFD(); 


