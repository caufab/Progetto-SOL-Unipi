/*
    Autore: Fabrizio Cau
    Matricola: 508700
    Corso A Informatica

    Sorgente Direttore
*/

#define _POSIX_C_SOURCE 200112L

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <mylib.h> 

volatile sig_atomic_t exitStatus;
pid_t storePid;

void maskAll(int m) {
    sigset_t set;
    SIG_ER("Direttore(sigfillset)", sigfillset(&set) ) 
    if(!m) {       
        SIG_ER("Direttore(sigdelset)", sigdelset(&set, SIGHUP) )
        SIG_ER("Direttore(sigdelset)", sigdelset(&set, SIGQUIT) )
    }
    SIG_ER("Direttore(pthread_sigmask)", pthread_sigmask(SIG_SETMASK, &set, NULL) )
}

void sighupHandler(int sig) {
    exitStatus = 1; // Normal exit: non entrano nuovi clienti e quelli già dentro concludono gli acquisti
    kill(storePid, sig);
    
}
void sigquitHandler(int sig) {
    exitStatus = 2; // fast exit: i clienti escono immediatamente
    kill(storePid, sig);
}

int main(int argc, char *argv[]) {

    CleanSFD(); 

    SetDefConf();
    ParseConfig(); // Aggiorna i valori default dal file config
    OverwriteConfig(argc,argv); // Setta eventuali parametri da stdin

    maskAll(1); // Maschera tutti i segnali
   
    // Se richiesto nei parametri di esecuzione, lancia il processo supermercato
    if(FORK_FROM_MNG) {
        FKE( storePid = fork() ) 
    } else storePid = 1; // Se non fa la fork entra comunque nel ramo else del if successivo

    if(storePid == 0){ // Supermercato
        execl("./bin/supermercato", "supermercato", "K", "6", "C", "50", "E", "3", \
        "T", "200", "P", "100", "CUST_MOVES_TIMEOUT", "20", (char*)NULL);
        Exit_ERR("Direttore: execl") // Se ritorna, stampa errore
     } else { // Direttore 
        int d, i, count = 0, j=0, *inCodaArr;
        struct sigaction s; 
        memset(&s, 0, sizeof(s));

        // Struttura code delle casse
        inCodaArr = malloc((K+1)*sizeof(int));
        if(!inCodaArr) MALLOC_ERR("Direttore(inCodaArr)")
        for(int i=0;i<K;i++)
            inCodaArr[i] = -1;
 
        // Struttura dati e connessione per comunicazione via socket
        int bufsize = 50;
        char buf[50]="";
        struct sockaddr_un sa;
        strncpy(sa.sun_path, SOCKETPATH, 9);
        sa.sun_family = AF_UNIX;
        SKT_ER( Skt_FD = socket(AF_UNIX, SOCK_STREAM, 0) )
        
        CONN_ER( connect(Skt_FD, (struct sockaddr*)&sa, sizeof(sa)) )
        
        RWE( read(Skt_FD, buf, bufsize) )
        if(strncmp(buf, "StorePid=", 9)) Exit_ERR("Manager: read 'StorePid'")

        sscanf(buf, "StorePid=%d", &storePid);
        
        s.sa_handler = sighupHandler;
        SIG_ER("Direttore(sigaction)", sigaction(SIGHUP, &s, NULL) )
        s.sa_handler = sigquitHandler;
        SIG_ER("Direttore(sigaction)", sigaction(SIGQUIT, &s, NULL) )

        maskAll(0); // Maschera tutti eccetto SIGHUP e SIGQUIT
  
        RWE( read(Skt_FD, buf, bufsize) ) // ready
        if(strcmp(buf, "ready")) Exit_ERR("Manager: read 'ready'")

        for(int i=0;i<INITIAL_CHECKOUTS;i++) {
            SP_ER( sprintf(buf, "ck-1:%d", i) )
            RWE( write(Skt_FD, buf, sizeof(buf)) )
            inCodaArr[i] = 0;
        }

        RWE( write(Skt_FD, "start", 6) )

        // Per divisione equa dei clienti in cassa, riassegno la soglia S2 (su casse disponibli, non aperte)
        if(MAX_CUST_PER_CKOUT>(C/K)) MAX_CUST_PER_CKOUT = (C/K);

        while(1) { 
            RWE( read(Skt_FD, buf, bufsize) )
            if(strncmp(buf, "cu", 2) == 0) { // Un cliente vuole uscire
                SP_ER( sscanf(buf, "cu-%d", &d) )
                if(d>=0 && exitStatus!=2) // exitStatus !=2 evita la pop in ritardo dopo la fastExit
                    RWE(write(Skt_FD, "cu-1", 5) ) // Alla lettura verrà fatta una pop su npe
                else fprintf(stderr, "errore lettura cliente in coda npe");
            } else if(strncmp(buf, "ck", 2) == 0) { // Una cassa comunica il suo dato inCoda
                SP_ER( sscanf(buf, "ck-%d:%d", &d, &i) )
                if(d>=0 && d<K && i>=0) { 
                    inCodaArr[d] = i; 
                    if(!exitStatus && i>MAX_CUST_PER_CKOUT) {
                        SP_ER( sprintf(buf, "ck-1:%d", (K)) ) // Apre una cassa random
                        RWE( write(Skt_FD, buf, sizeof(buf)) )
                    } else if(i<2 && exitStatus!=2) {
                        j=0;
                        count=0;
                        while(j<K && count<=MAX_EMPTY_CKOUTS) {
                            if(inCodaArr[j]!=-1 && inCodaArr[j]<2) count++;
                            j++;
                        }
                        if(count>=MAX_EMPTY_CKOUTS) {
                            SP_ER( sprintf(buf, "ck-0:%d", d) ) // Chiude la cassa d
                            RWE( write(Skt_FD, buf, sizeof(buf)) )
                            inCodaArr[d] = -1;
                        }
                    }
                } else fprintf(stderr, "errore lettura coda cassa\n");
            
            } else if(strncmp(buf, "store-lastmsg", 14) == 0) {
                RWE( write(Skt_FD, "mng-last", 9) )
                break;
            }
        }
        
        if(inCodaArr) free(inCodaArr);

        do {
            RWE( read(Skt_FD, buf, bufsize) )
        } while(strcmp(buf, "store-end"));

        RWE( close(Skt_FD) )

    }
    return 0;
}