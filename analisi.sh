#
# Autor: Fabrizio Cau
#


#!/bin/bash
export LC_NUMERIC="en_US.UTF-8"
FILE=logsupermercato.txt
if [ ! -f $FILE ]; then # Controllo esistenza file di log
    echo "ERROR: "$FILE" not found!"
    exit 1
fi
exec 3<$FILE # Apre il file FILE nel descrittore di file 3
Field_Separator=$IFS # Salva il field separator di default 
i=0
idCli=0
nProd=0
storeTime=0
ckTime=0
totCode=0
read -u 3 line # Memorizza la prima linea in line
IFS=, # Setta il separatore per scandire la riga letta dal file
read -a val <<< $line
echo $'\n'"Sono entrati "${val[0]}" clienti, prodotti venduti in totale: "${val[1]}
IFS=$Field_Separator # Riassegna il field separator di default per leggere un intera riga dal file
read -u 3 line
echo $line #Stampa "Clienti:"
while read -u 3 line ; do
    if [ $line == Casse: ]; then # Appena legge "Casse:" esce
        break
    fi
    IFS=, 
    read -a val <<< $line 
    idCli=${val[0]}
    nProd=${val[1]}
    # Esprime i valori letti in ms in secondi, con 3 cifre decimali:
    storeTime=$(printf "%1.3f" $(echo "((${val[2]}+${val[3]}))/1000" | bc -l)) # Somma il tempo di acquisto e tempo alle casse
    ckTime=$(printf "%1.3f" $(echo "${val[3]}/1000" | bc -l))
    totCode=${val[4]}       
    echo $'\t'"| idCli: $idCli | nProd: $nProd | storeTime: $storeTime s | ckTime: $ckTime s | totCode: $totCode |"    
    IFS=$Field_Separator    
done
echo $'\n'
#Parsing dati casse:
idCassa=0
totProd=0
totServ=0
closCount=0
echo $line #Stampa "Casse:"
#Stampa i dati delle casse
while read -u 3 line ; do
    IFS=,
    read -a val <<< $line
    idCassa=${val[0]}
    totProd=${val[1]}
    totServ=${val[2]}
    closCount=${val[3]}
    sumServTime=0
    avgServTime=0
    if [ $totServ -gt 0 ]; then # Se ha servito clienti, calcola la media dei tempi 
        IFS=$'\n'
        read -u 3 line
        line=${line#*ST:}
        IFS=,
        read -a st <<< $line
        for ((j=0; j<$totServ; j++)) do
            (( sumServTime+=${st[$j]} )) # Somma dei tempi di servizio
        done
        (( avgServTime=$sumServTime/$totServ )) # Media dei tempi di servizio
    fi
    closTimeSum=0
    if [ $closCount -gt 0 ]; then # Se ha chiuso almeno una volta, calcola la somma dei tempi di apertura
        IFS=$'\n'
        read -u 3 line
        line=${line#*CT:}
        IFS=,
        read -a st <<< $line
        for ((j=0; j<$closCount; j++)) do
            (( closTimeSum+=${st[$j]} )) # Somma dei tempi delle aperture
        done
    fi 
    avgServTime=$(printf "%.3f" $(echo "$avgServTime/1000" | bc -l))
    closTimeSum=$(printf "%.3f" $(echo "$closTimeSum/1000" | bc -l))
    echo $'\t'"| nC: $idCassa | TotP: $totProd | TotServ: $totServ | TotOpenTime: $closTimeSum s | AvgServTime: $avgServTime s | ClosCount: $closCount |"
done
echo $'\n'
IFS=$Field_Separator
