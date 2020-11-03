# Tema 1 Retele De Calculatoare

Acest document are rolul de a detalia tema pe care am facut-o si de a explica si exemplifica cum merge aceasta.

Folderul root va contine doua fisiere : "tema.c", "script.sh" si "bd.txt". In fisierul "tema.c" se afla codul propriu-zis iar fisierul "script.sh" are rolul de a se ocupa cu stergerea unor dependente(fisere) pe care programul "tema.bin" le creaza si de rulare a programului principal iar in fisierul "bd.txt" se afla username-urile utilizatorilor cu drept de a accesa aplicatia.

Scopul temei este de a realiza un program care simuleaza un bash. Se citesc comenzi in procesul tata si se executa in procesul/procesele.

Un utilizator al aplicatiei poate executa diferite tipuri de comenzi :

- Comenzi neprivilegiate : nu este nevoie sa te loghezi pentru a executa aceste comenzi (login,quit,help)
- Comenzi privilegiate interne : este nevoie sa te loghezi pentru a executa acest tip de comenzi iar aceste comenzi sunt create de program.(myfind,mystat)
- Comenzi privilegiate externe : este nevoie sa te loghezi pentru a executa acest tip de comenzi sunt comenzi specifice bash-ului de linux(find,stat,ls,wc etc.)

# Arhitectura programului

- Se porneste programul iar tatal va citi pe rand comenzi de la tastatura.
- Acesta va trimite comanda copilului fara a o altera in vreun fel.
- Copilul va citi comanda primita de la parinte, o va executa si ii va trimite raspunsul parintelui care il va afisa.

Pentru a asigura functionalitatea de login si quit programul va crea un fisier "flag.txt" care va stoca un singur char :

- 0 : daca trebuie sa iesim din aplicicatie intrucat am dat comanda "quit" cand nu eram logati
- 1 : daca suntem "Idle" in aplicatie (nu suntem logati)
- 2 : daca suntem logati in aplicatie

O comanda reusita de login va scrie in fisierul "flag.txt" caracterul '2' iar o comanda de quit ne va deloga sau va iesi din aplicatie, in functie de statusul nostru curent.

# Comenzile aplicatiei

help : afiseaza informatii despre comenzi

```sh
help
```

login : cauta in fisierul bd.txt username-ul specificat iar daca exista va autentifica in aplicatie

```sh
login : username
```

quit : va delogheaza daca sunteti autentificati si scoate din aplicatie in caz contrar

```sh
quit
```

myfind : cauta un fisier intr-un director specificat si afseaza detalii despre acesta daca il gaseste

```sh
myfind cale numeFisier
```

mystat : afiseaza date despre un fisier specificat ca parametru

```sh
mystat caleFisier
```

Comenzi uzuale din bash-ul de linux. De mentionat este faptul ca aceste comenzi nu accepta posibilitatea de inlantuire

```sh
find .
wc tema.c -l
ls -l
```

Exemplu demonstrativ

Inainte de a rula fisierul "script.sh" asigurativa ca acesta are permisiune de executie. O comanda pentru a garanta acest lucru pentru el este

```sh
chmod +x script.sh
```

Rulare program

```sh
$ ./script.sh
./tema.bin: login : username
20 Invalid Credentials
./tema.bin: login : vlad
10 Logged In
./tema.bin: quit
21 You were logged out.
./tema.bin: login : vlad
10 Logged In
./tema.bin: myfind . tema.c
440 calea catre "tema.c" este ./tema.c
Time of last access : Tue Nov  3 17:05:29 2020
TIme of last modification : Sun Nov  1 21:50:22 2020
Time of last status change : Sun Nov  1 21:50:22 2020
File Permissions : -rw-rw-r--
Size : 20432
Inode number : 3319848
ID of device containing file : 66310
Number of hard links : 1
User ID of owner : 1000
Group ID of owner : 1000
Blocksize for file system I/O : 4096
Number of 512B blocks allocated : 40
./tema.bin: find .
./tema.c
./bd.txt
./SEND_COMMAND_TO_CHILDREN
./flag.txt
./tema.bin: mystat
19 Invalid parameter.
./tema.bin: quit
21 You were logged out.
./tema.bin: quit
6 Exit.
```

# Resurse folosite in realizarea acestui proiect

- https://man7.org/linux/man-pages/man2/dup.2.html
- https://stackoverflow.com/questions/10323060/printing-file-permissions-like-ls-l-using-stat2-in-c
- https://profs.info.uaic.ro/~vidrascu/
- https://man7.org/linux/man-pages/man2/socketpair.2.html
- https://man7.org/linux/man-pages/man3/mkfifo.3.html
