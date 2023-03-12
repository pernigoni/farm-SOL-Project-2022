#!/bin/bash

if [ ! -e bin/farm ];
then
    echo "Compilare farm, eseguibile mancante!"
    exit 1
fi

YELLOW=`tput setaf 3`
BOLD=`tput bold`
RESET=`tput sgr0`

#
# Viene inviato SIGUSR1 per tre volte, infine SIGQUIT.
# Tra un segnale e l'altro passano circa 1, 2, 0.1 e 7 secondi quindi dovremmo avere 4 stampe.
#
echo "${YELLOW}${BOLD}=== TEST SIGUSR1 ===${RESET}"
./bin/farm -n 6 -q 3 -d . src/farm.c src libs ./Makefile Makefile -t 500 &
pid=$!
sleep 1
pkill -USR1 farm
sleep 2
pkill -USR1 farm
sleep 0.1
pkill -USR1 farm
sleep 7
pkill -QUIT farm
wait $pid

#
# Esecuzione con argomenti di default.
#
echo "${YELLOW}${BOLD}=== TEST DEFAULT ===${RESET}"
./bin/farm -d .

#
# Esecuzione con valgrind.
#
echo "${YELLOW}${BOLD}=== TEST VALGRIND ===${RESET}"
valgrind --leak-check=full ./bin/farm -n 8 -q 4 -d .
