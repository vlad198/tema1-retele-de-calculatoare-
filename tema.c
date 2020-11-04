#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdlib.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <limits.h>

#define FIFO_NAME "SEND_COMMAND_TO_CHILDREN"
#define NR_COMENZI_LOCALE 2
#define NR_COMENZI_NEPRIVILEGIAT 3

char comenzi_locale[4][50] = {"myfind", "mystat"};
char comenzi_neprivilegiate[4][50] = {"login", "quit", "help"};
char cuvinte_comanda[50][100000];
char comanda[100000] = {0};
int nrCuvinte;
int tip;

int p[2];     // pipe
int sockp[2]; // socket pair

int fd_flag;

void creaza_canale();

int isFolder(char *cale);
void myFind(char *cale, char *nume, char *rez);

void sparge_comanda_cuvinte(char sir[]);
int tip_comanda(char *comanda);

void copil_comenzi_neprivilegiate();
void copil_comenzi_privilegiate_interne();
void copil_comenzi_privilegiate_externe();

void parinte();

int main(int argc, char **argv)
{
    fd_flag = open("flag.txt", O_RDWR | O_CREAT | O_TRUNC, 0777);

    if (fd_flag == -1)
    {
        perror("Eroare la deschidere fisier pt flag.");
        exit(1);
    }

    // facem utilizatorul programului sa fie IDLE la inceuput
    if (write(fd_flag, "1", strlen("1")) != 1)
    {
        perror("Eroare la scriere in fisier flag.");
        exit(1);
    }

    char status; // ne va spune ce status avem la aplicatia noastra 0 - iesim 1 - suntem idle 2 - suntem logati

    while (1)
    {
        // citesc flag-ul din fisierul partajat intre procese
        if (lseek(fd_flag, 0, SEEK_SET) == -1)
        {
            perror("Eroare la lseek");
            exit(1);
        }

        // citim statusul dupa fiecare comanda
        int bytes_read = read(fd_flag, &status, 1);

        // eroare la citire
        if (bytes_read == -1)
        {
            perror("Eroare la citire");
            exit(0);
        }

        // EOF sau am dat comanda quit de 2 ori daca eram logat sau 1 data daca nu
        if (bytes_read == 0 || status == '0') // daca e 0 iesim
            break;

        // citim comanda pana la newline

        int lg = 0;
        char caracter;

        comanda[0] = NULL;

        // afisam calea curenta catre programul nostru
        printf("%s: ", argv[0]);

        // citim comanda pe care urmeaza sa o executam
        while (lg < 100000)
        {
            // citim caracter cu caracter
            scanf("%c", &caracter);

            // cand am ajuns la endline ne oprim
            if (caracter == '\n')
                break;
            else
            {
                comanda[lg++] = caracter;
                comanda[lg] = NULL;
            }
        }

        // daca nu am introdus nici o comanda, reluam whileul intrucat nu avem ce sa executam
        if (strlen(comanda) == 0)
            continue;

        // spargem comanda in cuvinte
        sparge_comanda_cuvinte(comanda);

        // vedem tipul comenzii neprivilegiata(login,quit),privilegiata interna(MyStat,MyFind), privilegiata externa(find,stat,cat etc)
        tip = tip_comanda(cuvinte_comanda[0]);

        // restrictionam accesul la executia comenzilor interne si externe atunci cand nu suntem logati
        if (tip >= 1 && status == '1')
        {
            printf("%d Access denied.\n", strlen("Access denied.\n"));
            continue;
        }

        // cream toate canalele de comunicatie intre procese
        creaza_canale();

        // cream procesul fiu care se va ocupa de executia comenzii
        switch (fork())
        {
        case -1:
            perror("Eroare la fork.");
            exit(1);
        case 0: // fiu
            // in functie de tipul comenzii vom apela o functie specifica care se ocupa de executia comenzii respective
            switch (tip)
            {
            case 0:
                // citeste din pipe scrie in socket
                copil_comenzi_neprivilegiate();
            case 1:
                // citeste din fifo scrie in socket
                copil_comenzi_privilegiate_interne();
            default:
                // citeste din fifo scrie in socket
                copil_comenzi_privilegiate_externe();
            }

        default: // suntem pe parinte
            // citim din socket scriem pe ecran
            parinte();
        }

        remove(FIFO_NAME); // stergem fifo-ul la fiecare executie a comenzii intrucat nu vrem sa ramana eventual ceva date in el si sa fie folosit la viitoarele comenzi
    }

    remove("flag.txt"); // stergem fisierul ajutator flag.txt care ne spunea statusul programului

    return 0;
}

void creaza_canale()
{
    if (pipe(p) == -1) // cream pipe-ul
    {
        printf("Eroare la pipe\n");
        exit(1);
    }

    if (mkfifo(FIFO_NAME, 0777) == -1) // cream fifo-ul
    {
        if (errno == EEXIST) // EEXIST=17 : valoarea lui errno pentru situația "File exists"
        {
            fprintf(stdout, "Observatie: fisierul fifo 'SEND_COMMAND_TO_CHILDREN' exista deja !\n");
            exit(1);
        }
        else
        {
            perror("Eroare la crearea fisierului fifo. Cauza erorii");
            exit(1);
        }
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockp) < 0) // cream socketpair-ul
    {
        perror("Err... socketpair");
        exit(1);
    }
}

void inchide_file_descriptor(int fd)
{
    if (close(fd) == -1)
    {
        perror("Eroare la inchiderea file descriptorului");
        exit(1);
    }
}

void copil_comenzi_neprivilegiate()
{
    // inchid capatul de scriere
    inchide_file_descriptor(p[1]);

    // inchid stdout-ul
    inchide_file_descriptor(1);

    // fac ca iesirea din stdout sa fie redirectionata in socket
    if (dup(sockp[1]) != 1)
    {
        fprintf(stderr, "dup - 1\n");
        exit(1);
    }

    // inchid fd pe care l-am duplicat
    inchide_file_descriptor(sockp[1]);

    char command[100000] = {0}, ch;
    int i = 0;

    while (1)
    {
        // citesc cate un caracter din pipe
        int code_read = read(p[0], &ch, 1);

        // daca code_read=-1 atunci a aparut o eroare la citire
        if (code_read == -1)
        {
            perror("Eroare la citire din pipe 2");
            exit(1);
        }

        // daca e 0 inseamna ca am citit tot ce am avut iar capatul de scriere a fost inchis
        if (code_read == 0)
            break;

        // formam din nou comanda
        command[i++] = ch;
    }

    char status;

    //iau statusul
    lseek(fd_flag, 0, SEEK_SET);
    read(fd_flag, &status, 1);

    if (strcmp(command, "help") == 0)
    {
        printf("Lista comenzi : \n");
        printf("login : username -> te autentifica in aplicatie si iti da acces la o serie de comenzi de executat\n");
        printf("quit -> te delogheaza daca esti logat sau iese din aplicatie in caz contrar\n");
        printf("myfind cale numeFisier -> cauta fisierul \'numeFisier\' in directorul \'cale'\ sau subdirectoarele acestuia si afiseaza date despre acesta daca il gaseste\n");
        printf("mystat caleFisier -> afiseaza informatii despre un fisier dat ca argument\n");
        printf("find,wc,ls etc. -> se pot executa comenzi specifice bash-ului de linux doar ca acestea nu pot fi inlantuite sau sa contina expresii regulate\n");
    }
    else if (strcmp(command, "quit") == 0)
    {
        // ma pozitionez din nou la inceputul fisierului
        lseek(fd_flag, 0, SEEK_SET);

        // decrementez statusul
        status = '0' + status - '1';

        // printf("status : %c\n", status);

        if (status == '1')
            printf("You were logged out.\n");
        else
            printf("Exit.\n");

        write(fd_flag, &status, 1);
    }

    else
    {
        // daca incerc sa fac o comanda de logare cand sunt deja logat pur si simplu voi ignora acest lucru
        if (status - '0' == 2)
        {
            printf("You are already logged in\n");
            exit(1);
        }

        // iau username-ul din comanda
        char *p = strtok(command, " :");
        p = strtok(NULL, ": ");

        // daca acesta nu este specificat ies cu un mesaj specific
        if (p == NULL)
            printf("Invalid Credentials\n");

        // deschid fisierul cu useri pentru a-l cauta pe userul specificat
        int fd_bd = open("bd.txt", O_RDONLY);

        // daca nu am putut deschide fisierul de useri ies
        if (fd_bd == -1)
        {
            perror("Eroare la deschiderea fisierului bd");
            exit(1);
        }

        char caracter, aux[100000] = {0};
        int found = 0;

        // caut userul
        while (1)
        {
            // citesc caracter cu caracter din fisier
            int code_read = read(fd_bd, &caracter, 1);

            // daca am o eroare sau am terminat de citit din fisier ies din bucla while
            if (code_read == -1 || code_read == 0)
                break;

            // daca nu intalnesc caracterul ' ' sau endl continui sa formez usernameul curent
            if (caracter != ' ' && caracter != '\n')
            {
                int lg_sir = strlen(aux);
                aux[lg_sir] = caracter;
                aux[lg_sir + 1] = NULL;
            }
            else
            {
                // daca nu, testez daca userul curent este cel specificat de utilizator iar daca da
                // intrerup cautarea acestuia iesind din bucla while
                if (strcmp(aux, p) == 0)
                {
                    found = 1;
                    break;
                }

                // reinitializam primul caracter cu NULL pentru a putea crea un nou posibil utilizator
                aux[0] = NULL;
            }
        }

        // testez daca fix ultimul utilizator este userul specifiat
        if (strcmp(aux, p) == 0)
            found = 1;

        // daca am gasit un user cu usernameul specifiat
        if (found == 1)
        {
            // updatam flagul in fisierul nostru
            lseek(fd_flag, 0, SEEK_SET);
            write(fd_flag, "2", 1);

            // scriem in socket mesajul comenzii
            printf("Logged In\n");
        }
        else
            printf("Invalid Credentials\n");
    }

    exit(2);
}

// https://stackoverflow.com/questions/10323060/printing-file-permissions-like-ls-l-using-stat2-in-c
void print_stat_atributes(char *comanda)
{
    struct stat st;

    // iau informatiile despre fisier/director cu un stat
    if (stat(comanda, &st) != 0)
    {
        printf("Eroare la stat pentru %s . \n", comanda);
        exit(1);
    }

    // scriu in socket detalii despre acest fisier
    printf("Time of last access : %s", ctime(&st.st_atime));
    printf("TIme of last modification : %s", ctime(&st.st_mtime));
    printf("Time of last status change : %s", ctime(&st.st_mtime));
    printf("File Permissions : ");
    printf((S_ISDIR(st.st_mode)) ? "d" : "-");
    printf((st.st_mode & S_IRUSR) ? "r" : "-");
    printf((st.st_mode & S_IWUSR) ? "w" : "-");
    printf((st.st_mode & S_IXUSR) ? "x" : "-");
    printf((st.st_mode & S_IRGRP) ? "r" : "-");
    printf((st.st_mode & S_IWGRP) ? "w" : "-");
    printf((st.st_mode & S_IXGRP) ? "x" : "-");
    printf((st.st_mode & S_IROTH) ? "r" : "-");
    printf((st.st_mode & S_IWOTH) ? "w" : "-");
    printf((st.st_mode & S_IXOTH) ? "x" : "-");
    printf("%c", '\n');
    printf("Size : %d\n", st.st_size);
    printf("Inode number : %d\n", st.st_ino);
    printf("ID of device containing file : %d\n", st.st_dev);
    printf("Number of hard links : %d\n", st.st_nlink);
    printf("User ID of owner : %d\n", st.st_uid);
    printf("Group ID of owner : %d\n", st.st_gid);
    printf("Blocksize for file system I/O : %d\n", st.st_blksize);
    printf("Number of 512B blocks allocated : %d\n", st.st_blocks);
}

void copil_comenzi_privilegiate_interne()
{
    // inchidem stdout
    inchide_file_descriptor(1);

    // fac ca iesirea din stdout sa fie redirectionata in socket
    if (dup(sockp[1]) != 1)
    {
        fprintf(stderr, "dup - 1\n");
        exit(1);
    }

    // inchid fd pe care l-am duplicat
    inchide_file_descriptor(sockp[1]);

    // deschid capatul de citire din fifo
    int fd_fifo = open(FIFO_NAME, O_RDONLY);

    // daca am avut eroare la deschiderea capatului de citire
    if (fd_fifo == -1)
    {
        perror("Eroare la deschiderea capatului de citire din fifo");
        exit(1);
    }

    char comanda[100000] = {0}, ch;
    int lg_comanda = 0;

    while (1)
    {
        // citesc cate un caracter
        int code_read = read(fd_fifo, &ch, 1);

        // daca am o eroare sau am terminat de citit din fifo  ies
        if (code_read == -1 || code_read == 0)
            break;

        // creez din nou comanda pe care am transmis-o prin canal
        comanda[lg_comanda++] = ch;
    }

    // sparg comanda in cuvinte
    sparge_comanda_cuvinte(comanda);

    // daca nu am un parametru pentru comanda ies
    if (nrCuvinte < 2)
    {
        printf("Invalid parameter.\n");
        exit(1);
    }

    // testez ce fel de comanda am introdus
    if (strcmp(cuvinte_comanda[0], "myfind") == 0)
    {
        char rez[100000] = {0};

        if (nrCuvinte < 3)
        {
            printf("Argumente insuficiente.\n");
            exit(1);
        }

        // caut fisierul/directorul specificat in directorul curent
        myFind(cuvinte_comanda[1], cuvinte_comanda[2], rez);

        // scriu in socket calea catre fisier
        if (strlen(rez) == 0)
        {
            printf("Fisierul '\%s'\ nu a fost gasit in directorul sau subdirectoarele \'%s\'\n", cuvinte_comanda[2], cuvinte_comanda[1]);
            exit(1);
        }

        printf("calea catre \"%s\" este %s\n", cuvinte_comanda[2], rez);

        print_stat_atributes(rez);
    }

    // testez ce fel de comanda am introdus
    if (strcmp(cuvinte_comanda[0], "mystat") == 0)
        print_stat_atributes(cuvinte_comanda[1]);

    exit(2);
}

void parinte()
{
    // inchid file descriptorul pt sockp[1] deoarece eu nu vrea sa fac nici o operatie
    // de scriere in acel socket si sa citesc in sockp[0]
    // daca nu fac aceasta voi avea readul blocant
    inchide_file_descriptor(sockp[1]);

    // ma duc pe o ramura in functie de tip
    switch (tip)
    {
    case 0: // comanda este neprivilegiata
        // inchid capatul de citire din pipe
        inchide_file_descriptor(p[0]);

        // o scriu in pipe
        int i = 0;
        while (i < strlen(comanda))
        {
            if (write(p[1], &comanda[i], 1) != 1)
                perror("Eroare la scrierea in pipe");
            i++;
        }

        inchide_file_descriptor(p[1]);

        break;
    case 1: // comanda este privilegiata
    case 2:
        printf("");

        // deschid capatul de scriere in fifo
        int fd_fifo = open(FIFO_NAME, O_WRONLY);

        // testez sa vad daca am avut vreo eroare la deschidere
        if (fd_fifo == -1)
        {
            perror("Eroare la deschiderea capatului de scriere in fifo.");
            exit(1);
        }

        // scriu comanda in fifo
        i = 0;
        while (i < strlen(comanda))
        {
            if (write(fd_fifo, &comanda[i], 1) != 1)
                perror("Eroare la scrierea in pipe");
            i++;
        }

        inchide_file_descriptor(fd_fifo);
        break;
    default:
        break;
    }

    char ch, raspuns[100000] = {0};

    // reconstitui raspunsul scris de copil
    int lg_raspuns = 0;
    while (1)
    {
        int code_read = read(sockp[0], &ch, 1);

        if (code_read == -1)
        {
            perror("Eroare la citire din socket 2");
            exit(1);
        }

        if (code_read == 0)
            break;

        raspuns[lg_raspuns++] = ch;
        raspuns[lg_raspuns] = NULL;

        // aici am facut ca sa immi afiseze mai multe linii decat atat cat am alocat eu in buffer
        // pentru a adauga ceva in +
        if (lg_raspuns == 100000)
        {
            printf("%d %s\n", lg_raspuns, raspuns);
            raspuns[0] = NULL;
            lg_raspuns = 0;
        }
    }

    // afisez raspunsul pe ecran
    if (lg_raspuns > 0)
        printf("%d %s", lg_raspuns, raspuns);

    // exit(2);
}

void sparge_comanda_cuvinte(char sir[])
{
    char cuv[100000] = {0};
    int ghilimele1 = 0;
    int ghilimele2 = 0;
    int lg = strlen(sir);
    nrCuvinte = 0;

    for (int i = 0; i < lg; i++)
    {
        if ((sir[i] == '\'' || sir[i] == '\"') && i > 0 && sir[i - 1] == '\\')
        {
            int n = strlen(cuv);
            cuv[n] = sir[i];
            cuv[n + 1] = NULL;
        }
        else if (sir[i] == '\'')
        {
            int n = strlen(cuv);
            cuv[n] = sir[i];
            cuv[n + 1] = NULL;
            if (ghilimele1 == 1 && ghilimele2 == 0)
            {
                strcpy(cuvinte_comanda[nrCuvinte++], cuv);
                cuv[0] = NULL;
                ghilimele1 = 0;
            }
            else
                ghilimele1 = 1;
        }
        else if (sir[i] == '\"')
        {
            int n = strlen(cuv);
            cuv[n] = sir[i];
            cuv[n + 1] = NULL;
            if (ghilimele2 == 1 && ghilimele1 == 0)
            {
                strcpy(cuvinte_comanda[nrCuvinte++], cuv);
                cuv[0] = NULL;
                ghilimele2 = 0;
            }
            else
                ghilimele2 = 1;
        }
        else if (sir[i] != ' ' || ghilimele1 == 1 || ghilimele2 == 1)
        {
            int n = strlen(cuv);
            cuv[n] = sir[i];
            cuv[n + 1] = NULL;
        }
        else if (strlen(cuv) > 0)
        {
            strcpy(cuvinte_comanda[nrCuvinte++], cuv);
            cuv[0] = NULL;
        }
    }

    if (strlen(cuv) > 0)
        strcpy(cuvinte_comanda[nrCuvinte++], cuv);
}

int tip_comanda(char *comanda)
{
    for (int i = 0; i < NR_COMENZI_NEPRIVILEGIAT; i++)
        if (strcmp(comenzi_neprivilegiate[i], comanda) == 0)
            return 0;

    for (int i = 0; i < NR_COMENZI_LOCALE; i++)
        if (strcmp(comenzi_locale[i], comanda) == 0)
            return 1;

    return 2;
}

void copil_comenzi_privilegiate_externe()
{
    // deschid capatul de citire din fifo
    int fd_fifo = open(FIFO_NAME, O_RDONLY);

    // testez daca am avut eroare la deschiderea acestuia
    if (fd_fifo == -1)
    {
        perror("Eroare la deschiderea capatului de citire din fifo");
        exit(1);
    }

    char comanda[100000] = {0}, ch;
    int lg_comanda = 0;

    // reconstitui comanda scrisa in fifo
    while (1)
    {
        int code_read = read(fd_fifo, &ch, 1);

        if (code_read == -1 || code_read == 0)
            break;

        comanda[lg_comanda++] = ch;
    }

    // sparg comanda in cuvinte/argumente
    sparge_comanda_cuvinte(comanda);

    // inchidem stdout
    inchide_file_descriptor(1);

    // fac ca iesirea din stdout sa fie redirectionata in socket
    if (dup(sockp[1]) != 1)
    {
        fprintf(stderr, "dup - 1\n");
        exit(1);
    }

    // inchid fd pe care l-am duplicat
    inchide_file_descriptor(sockp[1]);

    // creez array-ul de argumente dat la functia execvp cu, cuvintele din comanda transmisa
    char *arr[100000];
    for (int i = 0; i < nrCuvinte; i++)
        arr[i] = cuvinte_comanda[i];

    // pun pe ultimul cuvant din array NULL pentru a putea fi recunoscut de functia execvp
    arr[nrCuvinte] = NULL;

    // executam comanda
    execvp(arr[0], arr);

    // daca a fost vreo problema la aceasta
    printf("Invalid command.\n");
    exit(2);
}

void myFind(char *cale, char *nume, char *rez)
{
    DIR *dir;
    struct dirent *de;
    char caleNoua[PATH_MAX];
    int verifica;

    verifica = isFolder(cale);

    if (verifica == 0)
        return;
    else
    {
        if (NULL == (dir = opendir(cale)))
            return;
        else
        {
            while (NULL != (de = readdir(dir)))
            {
                if (strcmp(de->d_name, ".") && strcmp(de->d_name, ".."))
                {
                    sprintf(caleNoua, "%s/%s", cale, de->d_name);
                    if (strcmp(de->d_name, nume) == 0)
                        strcpy(rez, caleNoua);

                    myFind(caleNoua, nume, rez);
                }
            }
        }
    }
}

int isFolder(char *cale)
{
    int ok = 0;
    struct stat st;

    if (stat(cale, &st) != 0)
        return 0;

    if ((st.st_mode & __S_IFMT) == __S_IFDIR)
        ok = 1;

    return ok;
}