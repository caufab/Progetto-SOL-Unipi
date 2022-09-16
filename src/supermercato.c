/*
    Autore: Fabrizio Cau
    Matricola: 508700
    Corso A Informatica 
 
    Sorgente Supermercato
*/

#define _POSIX_C_SOURCE 200112L

/* ==== LIBRERIE ==== */
#include <mylib.h>
#include <datastruct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>


pthread_t tidManagInt,mngTid,tidGenCust,tidQuit;

void maskAll() {
    sigset_t set;
    SIG_ER("Supermercato(sigfillset)", sigfillset(&set) ) 
    SIG_ER("Supermercato(pthread_sigmask)", pthread_sigmask(SIG_SETMASK, &set, NULL) )
}

// ======== GESTIONE CASSE ==========================================================
static void* cassaSubThread(void *arg) { // Thread supporto cassa per notificare la coda al direttore
    cassa_t *c = ((cassa_t*)arg);
    if(c==NULL) ARG_ERR("cassaSubThread(NULL)")    
    char buf[20]="";
    ts_t now; 
    while(checkoutThreadStatus) {
        sleepms(INCODA_NOTIFY_TIMEOUT);       
        Lock(&c->mtx);        
            while(!c->isOpen && checkoutThreadStatus)
                Wait(&c->cond, &c->mtx); 
            CLK_ER( clock_gettime(CLOCK_REALTIME, &now) ) // Tempo attuale, quando viene risvegliato dalla wait
            if(checkoutThreadStatus && (GetTimeDiff(c->tmp,now)>=INCODA_NOTIFY_TIMEOUT) ) { // Notifica se non si è in chiusura o la cassa è stata appena aperta
                SP_ER( sprintf(buf, "ck-%d:%d", c->Ncassa, c->inCoda) )
                RWE( write(SktCon_FD, buf, sizeof(buf)) )
            }
        Unlock(&c->mtx);
    }
    return (void*) 0;
}

static void* threadCassa(void *arg) { // Gestione cassa 
    cassa_t *c = ((cassa_t*)arg); 
    if(c==NULL) ARG_ERR("threadCassa(NULL)")
    pthread_t subTid;
    TE( pthread_create(&subTid, NULL, &cassaSubThread, (void*) c) )
    while(checkoutThreadStatus) {  
        Lock(&c->mtx);
            while( c->inCoda==0 && checkoutThreadStatus) 
                Wait(&c->cond, &c->mtx); 
        Unlock(&c->mtx); 
        if(checkoutThreadStatus)
            popCliente(c);
    } 
    Signal(&c->cond);
    TE( pthread_join(subTid, NULL) ) 
    return (void*) 0;
}
// ==================================================================================


// ======== GESTIONE DIRETTORE ======================================================
static void* ManagerInterface(void* NoArg) { // Gestore direttore      
    initCasse();
    initNPE();
    for(int i=0;i<K;i++)
        TE( pthread_create(&Casse[i].tidCassa, NULL, &threadCassa, &Casse[i]))
    char buf[50]="";
    int bufsize = 50, nc;

    SP_ER( sprintf(buf, "StorePid=%d", getpid()) )
    RWE( write(SktCon_FD, buf, sizeof(buf)) )

    RWE( write(SktCon_FD, "ready", 6) )
    do {
        RWE( read(SktCon_FD, buf, bufsize) )
        if(strncmp(buf, "ck-1", 4) == 0) {
            SP_ER( sscanf(buf, "ck-1:%d", &nc) )
            apriCassa(nc);
        }
    } while(strcmp(buf, "start"));

    entranteStatus = 1; // Possono entrare i clienti
    
    while(1) { 
        RWE( read(SktCon_FD, buf, bufsize) )
        if(strcmp(buf, "cu-1") == 0)
            popCliente(npe);
        else if(strncmp(buf, "ck-1", 4) == 0) {
            SP_ER( sscanf(buf, "ck-1:%d", &nc) )
            apriCassa(nc);
        } else if(strncmp(buf, "ck-0", 4) == 0) {
            SP_ER( sscanf(buf, "ck-0:%d", &nc) )
            if(nc<K && nc>=0)
                chiudiCassa(nc);
            else fprintf(stderr, "errore lettura cassa da chiudere\n");
        } else if(strcmp(buf, "mng-last") == 0)
            break;
    }    

    for(int i=0;i<K;i++) {
        Broadcast(&Casse[i].cond); 
        TE( pthread_join(Casse[i].tidCassa, NULL) )
    }
    return (void*) Casse;
}
// ==================================================================================


// ======== GESTIONE CLIENTI ========================================================
static void* custChangeCkout(void* arg) { // Periodicamente controlla e sposta il cliente in altre casse
    cliente_t *Cli = ((cliente_t*)arg);
    if(Cli==NULL) ARG_ERR("custChangeCkout")
    int newC;  
    while(custThreadStatus) { // In chiusura i clienti si spostano al più un ultima volta
        sleepms(CUST_MOVES_TIMEOUT);
        Lock(&Cli->mtx);
            while( (Cli->status!=2 || Cli->CurCkout==-1) && custThreadStatus) // Attendo se il cliente non è in cassa, oppure lo è ma in npe
                Wait(&Cli->cond, &Cli->mtx);    
            if(custThreadStatus && Cli->CurCkout!=-1) {
                newC = bestCheckout(Cli->pos, Cli->CurCkout); // Trova una cassa migliore   
                if(newC!=Cli->CurCkout) 
                    if(extractCust(Cli, Cli->CurCkout)) {
                        Cli->CurCkout = newC;
                        pushCliente(Cli, Cli->CurCkout);                    
                        Cli->TotCode++;
                    }
            Signal(&Cli->cond);
            }   
        Unlock(&Cli->mtx); 
    }
    return (void*) 0;
}

static void* threadCliente (void* arg) { // Gestore slot cliente
    cliente_t *Cli = ((cliente_t*)arg);
    if(Cli==NULL) ARG_ERR("threadCliente")
    unsigned int t=0, p=0, cr=0, seed = 0;
    int *Opened = NULL; 
    pthread_t custChangeCkoutTid;
    ts_t start, stop;
    
    TE( pthread_create(&custChangeCkoutTid, NULL, &custChangeCkout, (void*) Cli) )

    while(custThreadStatus) {
        Lock(&Cli->mtx);
            Cli->status = 1;
            Cli->TotCode = 0;
        Unlock(&Cli->mtx);
        seed = (int)time(NULL)+Cli->idCli; 
        
        t = 10+(rand_r(&seed)%(T-10)); 
        p = rand_r(&seed)%P; 
        Opened = listCasse(1);
        if(!p) cr = -1; // 0 prodotti => cassa npe
        else cr = Opened[1+(rand_r(&seed)%(Opened[0]))];   // Cassa random tra quelle aperte
        free(Opened);
        sleepms(t);
        if(!fastExit) { // Se già entrato ormai fa la push
            Lock(&Cli->mtx);
                Cli->ShopTime = t;
                Cli->nProd = p;
                Cli->CurCkout = cr;
                Cli->TotCode++; 
                Signal(&Cli->cond);
            Unlock(&Cli->mtx);
            pushCliente(Cli,cr); // Push cliente Cli in coda alla cassa cr
            if(cr == -1) {
                char buf[20]="";
                sprintf(buf, "cu-%d", Cli->idCli);
                RWE( write(SktCon_FD, buf, sizeof(buf)) )
            }
            CLK_ER( clock_gettime(CLOCK_REALTIME, &start) )
            
            Lock(&Cli->mtx);
                if(Cli->status==1)
                    Cli->status = 2; // Cliente è in coda
                Broadcast(&Cli->cond); 
                while(Cli->status!=0 && !fastExit) // La fast exit non aspetta che cambi lo status del cliente
                    Wait(&Cli->cond, &Cli->mtx);
                CLK_ER( clock_gettime(CLOCK_REALTIME, &stop) )
                Cli->CheckoutTime=GetTimeDiff(start,stop);
            Unlock(&Cli->mtx);
            PushCustStats(Cli); // Push cliente Cli nelle statistiche
    
        Lock(&wcMtx);
            current--;
            Signal(&genCond);
            while(!waitingLen && custThreadStatus)
                Wait(&wcCond, &wcMtx);
            if(custThreadStatus) {
                Cli->idCli = waitingCustomers[waitingLen-1];
                waitingLen--;
                current++;
            }
        Unlock(&wcMtx);
        }
    } 
    Signal(&Cli->cond);
    TE( pthread_join(custChangeCkoutTid, NULL) )
    return (void *) 0;
}

static void* genCustomers(void* arg) { // gestione ingresso clienti 
    cliente_t *c = initClienti(); 
    if(c==NULL) Exit_ERR("initClienti")
    waitingCustomers = initWaitingCustomers();
    if(waitingCustomers==NULL) Exit_ERR("initWaitingCustomers")
    waitingLen = 0;
    current = 0;
    for(int i=0;i<C;i++) { // generazione iniziale
        c[i].idCli = i;
        CustCount++;
        current++;
        TE( pthread_create(&c[i].tidCliente, NULL, &threadCliente, &c[i]) )
    }
// Si risveglia ogni volta che un cliente esce e controlla current
    while(entranteStatus) {  
        Lock(&wcMtx);
            while(current>(C-E) && entranteStatus)
                Wait(&genCond, &wcMtx); 
            if(waitingLen==0 && entranteStatus) {
                CustCount+=E;
                for(int i=0;i<E;i++) {         
                    waitingLen++;
                    waitingCustomers[i] = (CustCount-i-1);                    
                }
                Broadcast(&wcCond); 
            }
        Unlock(&wcMtx);
    }
    waitingLen = 0;
    
    for(int i=0;i<C;i++) {
        if(fastExit) Broadcast(&c[i].cond);
        TE( pthread_join(c[i].tidCliente, NULL) )
    }
    return (void*) c;
}
// ==================================================================================

// ======== GESTIONE CHIUSURA =======================================================
static void* quit(void* arg) {
    int sig;
    sigset_t endSig;
    SIG_ER("quit: sigemptyset", sigemptyset(&endSig) )
    SIG_ER("quit: sigaddset", sigaddset(&endSig, SIGHUP) )  // Normal exit
    SIG_ER("quit: sigaddset", sigaddset(&endSig, SIGQUIT) ) // Fast exit

    SIG_ER("quit: sigwait", sigwait(&endSig, &sig) )

    cliente_t *cust;
    cassa_t *css;
    if(entranteStatus) {
        entranteStatus = 0; // Ultimo giro del ciclo di generazione clienti
        if(sig==SIGQUIT) { fastExit = 1; custThreadStatus = 0; checkoutThreadStatus = 0; }
        Signal(&genCond); // Signal al generatore di clienti
        custThreadStatus = 0; // Ultimo giro dei thread clienti   
        Broadcast(&wcCond); // Broadcast ai thread clienti che aspettano nuovi elementi   
        TE( pthread_join(tidGenCust, (void*) &cust) )
        if(waitingCustomers) free(waitingCustomers); // Libera i clienti che attendevano fuori
        custThreadStatus = 1; // Rimette a 1 per liberare solo dopo la struttura dati dei clienti
    }
    checkoutThreadStatus = 0; // Ultimo giro delle casse e loro subthread

    int *stillOp = listCasse(1);
    for(int i=1;i<=stillOp[0];i++)
        chiudiCassa(stillOp[i]);
    free(stillOp);

    RWE( write(SktCon_FD, "store-lastmsg", 14) )

    TE( pthread_join(tidManagInt, (void*) &css) ) // Attende che esca il manager

    if(custThreadStatus) free(cust); // Se i clienti erano entrati libera la loro struttura
    if(npe->Head) free(RecFreeNodeStats(npe->Head));
    if(npe) free(npe); // Libera la coda uscita senza acqusiti  
    PrintLog(); // Stampa il log prima di liberare il resto della memoria
    for(int i=0;i<K;i++) {        
        if(css[i].Head) free(RecFreeNodeOnly(css[i].Head)); // Se vi è l'uscita rapida, dealloca le code alle casse
        if(css[i].stHead) free(RecFreeTimesNode(css[i].stHead)); // Libera la struttura dei tempi di servizio di ogni cliente servito da ciascuna cassa
        if(css[i].openTimeHead) free(RecFreeTimesNode(css[i].openTimeHead)); // Libera la struttura dei tempi di apertura delle casse
                
    }
    if(css) free(css); // Libera le casse
    if(CustStatsHead) free(RecFreeNodeStats(CustStatsHead)); // Libera la struttura dati delle statistiche
    
    RWE( write(SktCon_FD, "store-end", 10) )

    RWE( close(SktCon_FD) )

    return (void*) 0;
}
// ==================================================================================


int main(int argc, char *argv[]) {

    maskAll();

    CleanSFD(); 
    SetDefConf(); 
    ParseConfig(); 
    OverwriteConfig(argc,argv);
    
    checkoutThreadStatus = 1;
    entranteStatus = 0;
    fastExit = 0;
    custThreadStatus = 0;
    
    // Variabili e strutture per i processi e la socket 
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCKETPATH, 9);
    sa.sun_family = AF_UNIX;
        
    // Inizializzazione socket:
    SKT_ER( Skt_FD = socket(AF_UNIX, SOCK_STREAM, 0) )
    SKT_ER( bind(Skt_FD, (struct sockaddr*)&sa, sizeof(sa)) )
    SKT_ER( listen(Skt_FD,SOMAXCONN) )

    SKT_ER( SktCon_FD = accept(Skt_FD, (struct sockaddr*)NULL, NULL) )

    RWE( close(Skt_FD) )

    // Interfaccia manager
    TE( pthread_create(&tidManagInt, NULL, &ManagerInterface, NULL) ) 

    while(!entranteStatus) // Attendi finché l'interfaccia del manager finisce l'inizializzazione delle casse
        sleepms(500);
    custThreadStatus = 1;
    
    // Generatore di clienti
    TE( pthread_create(&tidGenCust, NULL, &genCustomers, NULL) )

    // Gestore chiusura
    TE( pthread_create(&tidQuit, NULL, &quit, NULL) )

    TE( pthread_join(tidQuit, NULL) )
    
    return 0;
}



 

















