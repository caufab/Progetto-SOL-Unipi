/*
    Autore: Fabrizio Cau
    Matricola: 508700
    Corso A Informatica 
 
    Sorgente libreria datastruct (gestione struttura dati)
*/

#define _POSIX_C_SOURCE 200112L
#include <stdio.h> 
#include <stdlib.h>
#include <errno.h>
#include <datastruct.h>
#include <pthread.h>
#include <mylib.h>

// ===== INIZIALIZZAZIONE STRUTTURE DATI =====
void initCasse() {
    Casse = malloc(K*sizeof(cassa_t));
    if(!Casse) MALLOC_ERR("initCasse(Casse)") 
    unsigned int seed = time(NULL)+pthread_self();
    for(int i=0;i<K;i++) {
        Casse[i].Ncassa=i;
        Casse[i].inCoda=0;
        Casse[i].isOpen = 0;
        Casse[i].ClosingCount = 0;
        Casse[i].TotServiti = 0;
        Casse[i].TotProd = 0;
        Casse[i].baseServiceTime = 20+(rand_r(&seed)%(MAX_BASE_ST-20));
        Casse[i].stHead = NULL;
        Casse[i].stTail = NULL;
        Casse[i].openTimeHead = NULL;
        Casse[i].openTimeTail = NULL;
        Casse[i].Head = NULL;
        Casse[i].Tail = NULL;
        Casse[i].tidCassa = 0;
        InitMCCassa(&Casse[i]);
    }
}

void initNPE() {
    npe = malloc(sizeof(cassa_t));
    if(!npe) MALLOC_ERR("initNPE(npe)")
    npe->Ncassa = -1; // Indica che tiene la coda npe nella pop
    npe->inCoda = 0;
    npe->isOpen = 1;
    npe->baseServiceTime = 0;
    npe->Head = NULL;
    npe->Tail = NULL;
    InitMCCassa(npe);
}

cliente_t* initClienti() {
    cliente_t *c = malloc(C*sizeof(cliente_t)); 
    if(!c) MALLOC_ERR("initClienti(c)")
    for(int i=0;i<C;i++) {
        c[i].status = 0;
        c[i].CurCkout = 0;
        c[i].pos = 0;
        InitMCCli(&c[i]); 
    }
    return c;
} 

void initCustStats() {
    CustStatsHead = NULL;
    CustStatsTail = CustStatsHead;
    CustCount = 0;
    ProdCount = 0;
}

int* initWaitingCustomers() {
    int *w = malloc(E*sizeof(int)); 
    if(!w) MALLOC_ERR("initWaitingCustomers(w)")
    return w;
} 
// ===========================================

// ===== PUSH & POP ==========================
void PushCustStats(cliente_t *c) {
    if(c==NULL) ARG_ERR("PushCustStats")
    node_t *statsN = malloc(sizeof(node_t));
    if(!statsN) MALLOC_ERR("PushCustStats(statsN)")
    statsN->Cliente = malloc(sizeof(cliente_t));
    if(!statsN->Cliente) MALLOC_ERR("PushCustStats(statsN->Cliente)")
    *(statsN->Cliente) = *c; // Copia della struttura puntata da c
    statsN->next = NULL;
    Lock(&StatsMtx);
        if(CustStatsHead==NULL) CustStatsHead = statsN;
        else CustStatsTail->next = statsN;
        CustStatsTail = statsN;
        ProdCount += statsN->Cliente->nProd;
    Unlock(&StatsMtx);
}

void pushCliente(cliente_t *Cli, int nc) {
    if(Cli==NULL) ARG_ERR("pushCliente(Cli==NULL)")
    if(nc<-1 || nc >= K) ARG_ERR("pushCliente(nc out of bound)") 
    node_t *newN = malloc(sizeof(node_t));
    if(!newN) MALLOC_ERR("pushCliente(newN)")
    newN->Cliente = Cli;
    newN->next = NULL;
    cassa_t *c;
    if(nc == -1) c = npe;
    else c = &Casse[nc];
    Lock(&c->mtx); 
        if(c->Head == NULL) c->Head = newN;
        else c->Tail->next = newN;
        c->Tail = newN; 
        c->inCoda++;
        Cli->pos = c->inCoda;
        Broadcast(&c->cond); 
    Unlock(&c->mtx);   
}

int extractCust(cliente_t *Cli, int nc) {
    if(Cli==NULL) ARG_ERR("extractCust(Cli==NULL)")
    if(nc < 0 || nc >= K) ARG_ERR("extractCust(nc out of bound)")
    cassa_t *c = &Casse[nc];
    node_t *extrN, *oldN;
    short extracted = 0;
    Lock(&c->mtx);
        extrN = c->Head;
        if(extrN!=NULL) {
            if(extrN->Cliente != Cli) { // Se è primo in coda non si sposta(lo stanno servendo) 
                while(extrN->next!=NULL) {
                    if( ((extrN->next)->Cliente) == Cli ) {
                        oldN = extrN->next; // Per deallocare il nodo estratto
                        if(extrN->next==c->Tail) c->Tail = extrN;
                        else updatePositions((extrN->next)->next);
                        extrN->next = (extrN->next)->next;
                        extracted = 1;
                        c->inCoda--;
                        free(oldN);
                        break;
                    }     
                    extrN = extrN->next;
                }
            }
        }
        Broadcast(&c->cond);
    Unlock(&c->mtx);
    return extracted;
}
 
void popCliente(cassa_t *c) {   
    if(c==NULL) ARG_ERR("popCliente(NULL)")
    if(c->Head==NULL) ARG_ERR("popCliente(Head=NULL)")
    node_t *popN = c->Head;
    timesNode_t *stN = malloc(sizeof(timesNode_t));
    if(!stN) MALLOC_ERR("popCliente(stN)")
    cliente_t *cli = popN->Cliente; // Estrazione del cliente
    unsigned int t = c->baseServiceTime+(TIME_PER_PROD*cli->nProd);
    if(c->Ncassa!=-1) { 
        Lock(&cli->mtx);
            cli->status = 3; // Il cliente viene servito
            Broadcast(&cli->cond);
        Unlock(&cli->mtx);
        stN->t = t;
        stN->next = NULL;
    }
    else   
        free(stN);
    
    sleepms(t); // Attende il tempo di servizio
    // In ME rimuove il cliente dalla struttura dati
    Lock(&c->mtx);
        c->inCoda--;
        c->Head = c->Head->next;
        if(c->Ncassa!=-1) {
            c->TotServiti++;
            c->TotProd+=cli->nProd;
            if(c->stHead==NULL) c->stHead = stN;
            else c->stTail->next = stN;
            c->stTail = stN;
        }
        updatePositions(c->Head);
        Signal(&c->cond);
    Unlock(&c->mtx); 
    free(popN);
    // Setta lo status del cliente a 0 (uscito)
    Lock(&cli->mtx);
        cli->status = 0;
        Broadcast(&cli->cond);
    Unlock(&cli->mtx);
}
// ===========================================

// ==== APERTURA E CHIUSURA CASSE ============
void apriCassa(int n) {
    int *Closed = listCasse(0);
    if(Closed[0]!=0) {  // Se le casse non sono già tutte aperte
        if(!Exists(Closed, n))
            n = getRandom(Closed);
        Lock(&Casse[n].mtx);
            Casse[n].isOpen = 1;
            CLK_ER( clock_gettime(CLOCK_REALTIME, &Casse[n].tmp) )
            Broadcast(&Casse[n].cond);
        Unlock(&Casse[n].mtx);
    }
    free(Closed);
}

void chiudiCassa(int n) {
    int *Opened = listCasse(1);
    node_t *tmp;
    ts_t closeT;

    if(Opened[0]>1 || (Opened[0]==1 && !checkoutThreadStatus)) {
        if(!Exists(Opened, n))
            n = getRandom(Opened);  
        timesNode_t *tn = malloc(sizeof(timesNode_t));
        if(!tn) MALLOC_ERR("chiudiCassa(tn)")
        tn->next = NULL;
        CLK_ER( clock_gettime(CLOCK_REALTIME, &closeT) )
        tn->t = GetTimeDiff(Casse[n].tmp, closeT);
        Lock(&Casse[n].mtx);
            Casse[n].isOpen = 0;
            Casse[n].ClosingCount++;
            // Push di un nuovo tempo di apertura
            if(Casse[n].openTimeHead == NULL) Casse[n].openTimeHead = tn;
            else Casse[n].openTimeTail->next = tn;
            Casse[n].openTimeTail = tn;
            tmp = Casse[n].Head;
            while(tmp!=NULL) { // Signal a tutti i clienti in coda
                Signal(&(tmp->Cliente)->cond);
                tmp = tmp->next;
            }
            Signal(&Casse[n].cond);
        Unlock(&Casse[n].mtx);
    }
    free(Opened);
}
// ===========================================

// ===== FUNZIONI SUPPORTO ===================
int* listCasse(int isOp){
    if(isOp!=0 && isOp!=1) ARG_ERR("listCasse")
    int *list = malloc((K+1)*sizeof(int));
    if(!list) MALLOC_ERR("listCasse(list)")
    list[0] = 0;
    for(int i=0;i<K;i++)
        if(Casse[i].isOpen == isOp)
            list[++list[0]] = Casse[i].Ncassa;
    return list;
}

short stillOpen(int nc) {
    if(nc<-1 || nc>K) ARG_ERR("stillOpen(nc out of bound)")
    if(nc==-1) return 1;
    return Casse[nc].isOpen;
}

void updatePositions(node_t *h) {
    node_t *cur = h;
    while(cur!=NULL) {
        (cur->Cliente)->pos--;
        cur = cur->next;
    }
}

node_t* RecFreeNodeStats(node_t *n) {
    if(n!=NULL) {
        free(n->Cliente);
        free(RecFreeNodeStats(n->next));
    }
    return n;    
}

node_t* RecFreeNodeOnly(node_t *n) { 
    if(n!=NULL)
        free(RecFreeNodeOnly(n->next));
    return n;    
}

timesNode_t* RecFreeTimesNode(timesNode_t *ts) {
    if(ts!=NULL)
        free(RecFreeTimesNode(ts->next));
    return ts;     
}

int bestCheckout(int pos, int curCk) {
    if(pos<0 || curCk<-1 || curCk>=K) ARG_ERR("bestCheckout(out of bound)")
    cassa_t *c = Casse;
    int minCoda = pos, bestCk;
    if(!stillOpen(curCk)) {
        int *Opened = listCasse(1);
        curCk = getRandom(Opened);
        minCoda = C;
        free(Opened);
    }    
    bestCk = curCk;
    for(int i=0;i<K;i++) {
        if(c[i].isOpen && (c[i].inCoda<(minCoda+1))) { // Il cliente si sposta solo se la differenza è 2
            minCoda = c[i].inCoda;
            bestCk = i;
        }
    }
    return bestCk;
}
// ===========================================

// ===== STAMPA DEL FILE LOG =================
void PrintLog() {
    FILE *log;
    FE( (log=fopen(LOGFILE, "w+")) )
    node_t *cur = CustStatsHead;
    cliente_t *c;
    SP_ER( fprintf(log, "%d,%d\nClienti:\n", CustCount, ProdCount) )
    while(cur!=NULL) {
        if(cur->Cliente==NULL) Exit_ERR("PrintLog: cur->Cliente is NULL")
        c = cur->Cliente;
        SP_ER( fprintf(log, "%d,%u,%u,%lu,%u\n", c->idCli, c->nProd, c->ShopTime, c->CheckoutTime, c->TotCode) )
        cur = cur->next;
    }
    SP_ER( fprintf(log, "Casse:\n") )
    cassa_t *k;
    timesNode_t *cur2;
    for(int i=0;i<K;i++) {
        k = &Casse[i];
        SP_ER( fprintf(log, "%d,%d,%d,%d",k->Ncassa,k->TotProd,k->TotServiti,k->ClosingCount) )
        cur2 = k->stHead;
        if(k->TotServiti) { 
            SP_ER( fprintf(log, "\nST:") )
            while(cur2!=NULL) {
                SP_ER( fprintf(log,"%lu", cur2->t) )
                cur2 = cur2->next;
                if(cur2!=NULL) SP_ER( fprintf(log, ",") )
            } 
        }
        cur2 = k->openTimeHead;
        if(k->ClosingCount) { 
            SP_ER( fprintf(log, "\nCT:") )
            while(cur2!=NULL) {
                SP_ER( fprintf(log,"%lu", cur2->t) )
                cur2 = cur2->next;
                if(cur2!=NULL) SP_ER( fprintf(log, ",") )
            } 
        }
        SP_ER( fprintf(log, "\n") )
    }
    fclose(log);
}
// ===========================================

// ===== MUTEX & VAR COND ====================
inline void Lock(pthread_mutex_t *m) {
    MCE(pthread_mutex_lock(m), "lock")
}
inline void Unlock(pthread_mutex_t *m) {
    MCE(pthread_mutex_unlock(m), "unlock")
}
inline void Signal(pthread_cond_t *c) {
    MCE(pthread_cond_signal(c), "signal")
}
inline void Broadcast(pthread_cond_t *c) {
    MCE(pthread_cond_broadcast(c), "broadcast")
}
inline void Wait(pthread_cond_t *c, pthread_mutex_t *m) {
    MCE(pthread_cond_wait(c, m), "wait")
}

inline void InitMCCassa(cassa_t *c) {
    MCE(pthread_mutex_init(&c->mtx, NULL), "initMutexCassa")
    MCE(pthread_cond_init(&c->cond, NULL), "initCondCassa")
}
inline void InitMCCli(cliente_t *c) {
    MCE(pthread_mutex_init(&c->mtx, NULL), "initMutexCli")
    MCE(pthread_cond_init(&c->cond, NULL), "initCondCli")
}
// ===========================================