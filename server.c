/* Ursache Andrei - 322CA */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>

#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001
#define FILE_NAME_SIZE 255
#define MAX_FILES_NR 100
#define forever while(1)

/* Functii -------------------------------------------------------------------*/

/* Comanda CD */
void cd_command(char dir_name[], int mode);
/* Comanda LS */
void ls_command(char dir_name[], int mode);
/* Comanda CP */
void cp_command(char file_name[], int mode);
/* Comanda SN */
void sn_command(char file_name[], int mode);

/* Codifica in functie de mod mesajul pe care trebuie sa il trimita si il pune 
   in payload
   Intoarce dimensiunea actuala a payload-ului */
int coder(char* payload, int size, int mode);
int coder_simple(char* payload, int size);
int coder_parity(char* payload, int size);
int coder_hamming(char* payload, int size);

/* Decodifica mesajele primite in functie de mod
 Intoarce dimensiunea mesajului decodificat */
int decoder(char* payload, int len, int mode);
int decoder_simple(char* payload, int len);
int decoder_parity(char* payload, int len);
int decoder_hamming(char* payload, int len);

/* Functie suport pentru trimiterea ACK si NACK */
void send_feedback(char sgn[], int mode);
/* Trimite ACK */
void send_ACK(int mode);
/* Trimite NACK */
void send_NACK(int mode);
/* Functie care asteapta si returneaza feed-backul */
int receive_feedback(int mode);

/* Fuctie care pune in r un mesaj primit (obisnuit), fara decodificare */
void receive_message(msg* r);
/* Functie de primire a unui mesaj in mod simple */
void receive_simple(msg* r);
/* Functie de primire a unui mesaj in mod parity
   In final mesajul este cel corect */
void receive_parity(msg* r);
/* Functie de primire a unui mesaj in mod hamming
   In final mesajul este cel corect */
void receive_hamming(msg* r);
/* Functia generala de primire a unui mesaj
   Il si decodifica atunci cand il primeste pe cel corect */
void receive(msg* r, int mode);

/* Functia de trimitere a unui mesaj pana se primeste ACK */
void server_send_message(char payload[], int len, int mode);

/* Returneaza un vector cu argumentele unei comenzi (2 elemente) */
void parse_command(char* command, char* command_args[]);
/* Returneaza numarul de cirfre ale unui numar */
int count_digits(int n) ;
/* FUnctie care numara intrarile intr-un director */
int count_entries(DIR* dir);
/* Returneaza numarul de biti de 1 din c */
int nr_bits(char c);
/* 0 - in chr sunt un numar par de iti de unu, 1 - altfel */
int find_parity(char str[], int len);

/* Pune in res reprezentarea in binar a lui chr */
void char_to_binary(char chr, char res []);
/* Seteaza bitul de la pozitia poz la valoarea val */
void set_bit_value(unsigned short* word, int poz, char val);
/* Seteaza bitul de la pozitia poz la valoarea val */
void set_bit_value_char(char* buff, int poz, char val);
/* Schimba valoarea bitului de pe pozitia poz */
void change_bit_value(unsigned short* word, int poz);
/* Intoarce suma % 2 a bitilor din word controlati de control */
int get_control_sum(unsigned short word, int control);

/* Intrarea in program ----------------------------------------------MAIN-----*/

int main(int argc, char* argv[]) {
    msg r;
    
    // modul de rulare: 0 = "simple", 1 = "parity" sau 2 = "hamming"
    int mode; 
    
    // Initializare mode
    if(argc == 1){
        mode = 0;
    }
    else {
        if(strcmp(argv[1],"parity") == 0)
            mode = 1;
        else
            mode = 2;
    }
    
    printf("[RECEIVER] Starting.\n");
    init(HOST, PORT);
    
    forever {

        // Asteapta o comanda
        receive(&r, mode);
        char* command_args[2];
        
        // Extragere argumente comanda:
        parse_command(r.payload, command_args);
        
        // CD
        if( strcmp(command_args[0], "cd") == 0 ){
            cd_command(command_args[1], mode);
            continue;
        }
    
        // LS
        if( strcmp(command_args[0], "ls") == 0 ){
            ls_command(command_args[1], mode);
            continue;
        }
        
        // CP
        if( strcmp(command_args[0], "cp") == 0 ){
            cp_command(command_args[1], mode);
            continue;
        }
        
        // SN
        if( strcmp(command_args[0], "sn") == 0 ){
            sn_command(command_args[1], mode);
            continue;
        }
        
        // exit
        if( strcmp(command_args[0], "exit") == 0 ){
            break;
        }
        
    }
    printf("[RECEIVER] Finished receiving..\n");
    return 0;
}

/* Functii corespunzatoare comenzilor ----------------------------------------*/

/* Comanda CD */
void cd_command(char dir_name[], int mode){
    int res;
    res = chdir(dir_name);
    if(res < 0){
        perror("[RECEIVER] CD error. Exiting.\n");
        exit(-1);
    }
}

/* Comanda LS */
void ls_command(char dir_name[], int mode){
    // Deschide folder:
    DIR* dir;
    dir = opendir(dir_name);
    if(dir == NULL){
        perror("[RECEIVER] LS error. Can not open the directory. Exiting.\n");
        exit(-1);
    }
    
    int i = 0;
    char payload[MSGSIZE];
    i = count_entries(dir);
    sprintf(payload, "%d", i);
    
    // Se trimite num
    int len = coder(payload, count_digits(i)+1, mode);
    server_send_message(payload, len, mode);
    
    // Se trimit numele intrarilor:    
    struct dirent* drnt;
    while((drnt = readdir(dir)) != NULL) {
        memset(payload,0,MSGSIZE);
        memcpy(payload, drnt->d_name, strlen(drnt->d_name));
        len =  coder(payload, strlen(drnt->d_name) + 1, mode);
        // Se trimite mesajul
        server_send_message(payload, len, mode);
    }
}
    

/* Comanda CP */
void cp_command(char file_name[], int mode){

    // Se determina dimensiunea
    FILE* file = fopen(file_name, "rb");
    int file_size = 0;
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fclose(file);
    
    // Auxiliar pentru citirea din fisier
    int dim = 0;
    if(mode == 1){
        dim = 1;
    }
    if( mode == 2){
        dim = 700;
    }
    
    char payload[MSGSIZE];
    sprintf(payload, "%d", file_size);
    
    // Se trimite size
    int len = coder(payload, strlen(payload)+1, mode);
    server_send_message(payload, len, mode);
    
    // Se trimite continutul
    int file_id = open(file_name, O_RDONLY);
    int counter;
    memset(payload, 0, MSGSIZE);
    
    while ( (counter = read(file_id, payload, MSGSIZE - dim)) != 0 ){
        len  = coder(payload, counter, mode);
        server_send_message(payload, len, mode);
        memset(payload, 0, MSGSIZE);
    }
    
    close(file_id);
}


/* Comanda SN */
void sn_command(char file_name[], int mode){
    msg r;
    int size;
    
    // Primeste size-ul
    receive(&r, mode);
    sscanf(r.payload, "%d", &size);
    
    // Determinare nume
    char name[FILE_NAME_SIZE];
    strcpy(name, "new_");
    strcat(name, file_name);
    
    // Creeare fisier
    int file_id = creat (name, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP 
    | S_IROTH | S_IWOTH );
    
    // Primire mesaje si scriere in fisier
    while( size > 0) {
        receive(&r, mode);
        write(file_id, r.payload, r.len);
        // Pana cand mai sunt pachete de primit
        size-=r.len;
    }
    close(file_id);
}

/* Functii de codare a continuturilor ----------------------------------------*/

int  coder_simple(char* payload, int size){
    return size;
}

int coder_parity(char* payload, int size){
    int i;
    int parity_b = find_parity(payload, size);
    
    for ( i = size ; i > 0 ; --i){
        payload[i] = payload[i-1];
    }
    // Seteaza primul bit de paritate
    payload[0] = parity_b ? 1 : 0;
    size++;
    return size;
}


int coder_hamming(char* payload, int size){

    int i,j, k;
    char result [size*2];
    unsigned short word = 0;
    char binary[8];
    for(i = 0 ; i < size ; ++i){
    
        // Se obtine reprezentarea binara
        char_to_binary(payload[i], binary);
        
        word = 0;
        // Se seteaza primul bit 
        set_bit_value(&word, 9, binary[7]);
        
        k = 7;
        // Se seteaza restul de biti
        for(j = 6 ; j >= 0 ; j--){
            if(k == 4){
                k--;
            }
            set_bit_value(&word, k , binary[j]);
            k--;
        }
        // In acest moment in word e pus un byte din payload pe doi bytes 
        // iar toti bitii de control sunt 0.
        
        // Se seteaza bitii de control
        set_bit_value(&word, 11, get_control_sum(word, 1));
        set_bit_value(&word, 10, get_control_sum(word, 2));
        set_bit_value(&word, 8, get_control_sum(word, 4));
        set_bit_value(&word, 4, get_control_sum(word, 8));
        
        // Se pune word-ul in result
        result[2*i] = (word >> 8) & 0xff;
        result[2*i+1] = word & 0xff;
    }
    memset(payload, 0, MSGSIZE);
    memcpy(payload, result, size*2);
    memset(result, 0, size*2);
    return size * 2;
}

/* Codifica in functie de mod mesajul pe care trebuie sa il trimita si il pune 
   in payload
   Intoarce dimensiunea actuala a payload-ului */
int coder(char* payload, int size, int mode){
    switch(mode){
        case 0:
            return coder_simple(payload, size);
        case 1:
            return coder_parity(payload, size);
        case 2:
            return coder_hamming(payload, size);
    }
    
    return 0;
}

/* Functii de decodare a continuturilor --------------------------------------*/

int  decoder_simple(char* payload, int len){
    return len;    
}

int decoder_parity(char* payload, int len){
    int i ;
    for(i = 0 ; i < len-1  ; i++){
        payload[i] = payload[i+1];
    }
    payload[len-1] = 0;
    return len - 1;
}

int decoder_hamming(char* payload, int len){
    char buff[len / 2];
    int i, j, k;
    for(i = 0 ; i < len / 2 ; i++){
        // Se extrage primul bit
        set_bit_value_char(&buff[i], 7, (payload[2*i] >> 1) & 1);
        k = 7;
        
        // Se extrag restul de biti
        for(j = 6 ; j >= 0 ; j--){
            if(k == 4){
                k--;
            }
            set_bit_value_char(&buff[i], j ,(payload[2*i+1] >> k) & 1);
            k--;
        }
        
    }
    memset(payload, 0, MSGSIZE);
    memcpy(payload, buff, len/2);
    memset(buff, 0, len/2);
    
    return len / 2;        
}

/* Decodifica mesajele primite in functie de mod
 Intoarce dimensiunea mesajului decodificat */
int decoder(char* payload, int len, int mode){
    switch(mode){
        case 0:
            return decoder_simple(payload, len);
        case 1:
            return decoder_parity(payload, len);
        case 2:
            return decoder_hamming(payload, len);
    }
    
    return 0;
}

/* Functii de gestionare a feedback-urilor -----------------------------------*/

/* Functie suport pentru trimiterea ACK si NACK */
void send_feedback(char sgn[], int mode){
    msg r;
    strcpy(r.payload, sgn);
    r.len = strlen(r.payload);
    
    // Se codifica mesajul:
    if(mode == 1){
        coder(r.payload, r.len, mode);
        r.len++;
    }
    
    int res = send_message(&r);
    char msg[100];
    sprintf(msg,"[RECEIVER] Send %s error. Exiting.\n",sgn);
    
    if (res < 0) {
        perror(msg);
        exit(-1);
    }
    printf("[RECEIVER] Send %s\n",sgn);
    
}

/* Trimite ACK */
void send_ACK(int mode){
    char ack[4] = "ACK";
    send_feedback(ack, mode);
}

/* Trimite NACK */
void send_NACK(int mode){
    char nack[5] = "NACK";
    send_feedback(nack, mode);
}

/* Functie care asteapta si returneaza feed-backul
   0 - ACK si 1 NACK */
int receive_feedback(int mode){
    msg r;
    receive_message(&r);
    
    // Simple and Hamming
    if(mode == 0 || mode == 2){
        if(r.len == 3){
            return 0;
        }
        else{
            return 1;
        }
    }
    
    // Parity
    if(mode == 1){
        if(r.len == 4){
            return 0;
        }
        else{
            return 1;
        }
    }
    return 0;
}

/* Functii de gestionare a pachetelor primite --------------------------------*/

/* Fuctie care pune in r un mesaj primit (obisnuit), fara decodificare */
void receive_message(msg* r){
    int res;
    res = recv_message(r);
    if (res < 0) {
        perror("[RECEIVER] Receive error. Exiting.\n");
        exit(-1);
    }
    printf("[RECEIVER] Am primit mesajul: %s \n",r->payload);
    
}

/* Functie de primire a unui mesaj in mod simple */
void receive_simple(msg* r){
    receive_message(r);
    send_ACK(0);
}

/* Functie de primire a unui mesaj in mod parity
   In final mesajul este cel corect */
void receive_parity(msg* r){
    int test;
    do{
        receive_message(r);
        test = find_parity((r->payload+1), r->len-1);
        test += (r->payload[0] & 1);
        test %= 2;
        if(test == 1){
            send_NACK(1);
        }
        else {
            send_ACK(1);
        }
    // Pana cand se primeste corect
    } while(test == 1);
    
    // Se decodifica mesajul
    r->len = decoder(r->payload, r->len, 1);
}

/* Functie de primire a unui mesaj in mod hamming
   In final mesajul este cel corect */
void receive_hamming(msg* r){
    receive_message(r);
    send_ACK(2);
    
    // Corectare:
    int i;
    unsigned short word;
    int sum_1, sum_2, sum_4, sum_8;
    int poz;
    
    for(i = 0 ; i < r->len/2 ; i++){    
        word = 0;
        poz = 0;
        // Se formeaza un cuvant
        word = ( r->payload[2*i] << 8 ) & 0xff00;
        word |= (0x00ff & r->payload[2*i+1]);
        // Se determina sumele contrololate de bitii de control:        
        sum_1 = get_control_sum(word, 1);
        sum_2 = get_control_sum(word, 2);
        sum_4 = get_control_sum(word, 4);
        sum_8 = get_control_sum(word, 8);
        // Se determina bitul gresit
        if(sum_1){
            poz += 1;
        }
        if(sum_2){
            poz += 2;
        }
        if(sum_4){
            poz += 4;
        }
        if(sum_8){
            poz += 8;
        }
        // Daca exista:
        if(poz){
            change_bit_value(&word, 15 - (poz + 3));
            
            // Se pune word-ul in result
            r->payload[2*i] = (word >> 8) & 0xff;
            r->payload[2*i+1] = word & 0xff;
        }
    }
    
    // Se decodifica
    r->len = decoder(r->payload, r->len, 2);
}

/* Functia generala de primire a unui mesaj
   Il si decodifica atunci cand il primeste pe cel corect */
void receive(msg* r, int mode){
    switch(mode){
        case 0:
            receive_simple(r);
            break;
        case 1:
            receive_parity(r);
            break;
        case 2:
            receive_hamming(r);
            break;
    }
}



/* Functie prin care serverul trimite un mesaj -------------------------------*/
/*   Il trimite pana cand primeste ACK */
void server_send_message(char payload[], int len, int mode){
    msg s;
    memcpy(s.payload, payload, len);
    s.len = len;
    int res;
    
    do {
        res = send_message(&s);
        if (res < 0) {
            perror("[RECEIVER] Send error. Exiting.\n");
            exit(-1);
        }
    // Pana cand se primeste ACK
    } while( receive_feedback(mode) == 1);
}

/* Functii auxiliare ---------------------------------------------------------*/

/* Returneaza un vector cu argumentele unei comenzi (2 elemente) */
void parse_command(char* command, char* command_args[]){
    char* token;
    token = strtok(command, " ");
    command_args[0] = strdup(token);
    token = strtok(NULL, " ");
    command_args[1] = strdup(token);
}

/* Returneaza numarul de cirfre ale unui numar */
int count_digits(int n) {
    int count = 0;
    while(n > 0) {
        count++;
        n /= 10;
    }
    return count;
}

/* Functie care numara intrarile intr-un director */
int count_entries(DIR* dir){
    int counter = 0;
    struct dirent* drnt;
    while((drnt = readdir(dir)) != NULL){
        counter++;
    }
    // Repozitionare la inceput:
    rewinddir(dir);
    return counter;
}

/* Returneaza numarul de biti de 1 din c */
int nr_bits(char c){
    int count = 0;
    if (c) {
        do {
               count++; 
        } while (c &= c - 1);
    }
    
    return count;
}

/* 0 - in chr sunt un numar par de iti de unu, 1 - altfel */
int find_parity(char str[], int len){
    int i = 0;
    int counter = 0;
    for( i = 0 ; i < len ; ++i){
        counter+=nr_bits(str[i]);
    }
    
    return counter % 2;
}

/* Pune in res reprezentarea in binar a lui chr */
void char_to_binary(char chr, char res []){
    int c, k;
    for (c = 7; c >= 0; c--) {
        k = chr >> c;
        if (k & 1)
            res[c] = 1;
        else
            res[c] = 0; 
      }
}

/* Seteaza bitul de la pozitia poz la valoarea val */
void set_bit_value(unsigned short* word, int poz, char val){
    if(val){
        (*word) |= 1<<poz;
    }
    else{
        (*word) &= ~(1<<poz);
    }
}

/* Seteaza bitul de la pozitia poz la valoarea val */
void set_bit_value_char(char* buff, int poz, char val){
    if(val){
        (*buff) |= 1<<poz;
    }
    else{
        (*buff) &= ~(1<<poz);
    }
}

/* Schimba valoarea bitului de pe pozitia poz */
void change_bit_value(unsigned short* word, int poz){
        (*word) ^= 1<<poz;
}

/* Intoarce suma % 2 a bitilor din word controlati de control */
int get_control_sum(unsigned short word, int control){
    int i, sum=0;
    int poz[6], size;
    
    // in poz se vor pune bitii controlati de control
    switch(control){
        case 1:
            poz[0] = 11;
            poz[1] = 9;
            poz[2] = 7;
            poz[3] = 5;
            poz[4] = 3;
            poz[5] = 1;
            size = 6;
            break;
        
        case 2:
            poz[0] = 10;
            poz[1] = 9;
            poz[2] = 6;
            poz[3] = 5;
            poz[4] = 2;
            poz[5] = 1;
            size = 6;
            break;
        
        case 4:
            poz[0] = 8;
            poz[1] = 7;
            poz[2] = 6;
            poz[3] = 5;
            poz[4] = 0;
            size = 5;
            break;
        
        case 8:
            poz[0] = 4;
            poz[1] = 3;
            poz[2] = 2;
            poz[3] = 1;
            poz[4] = 0;
            size = 5;
            break;
    }
    
    // Se face suma lor
    for(i = 0 ; i < size ; i++){
        sum += ((word >> poz[i]) & 1);
    }
    
    return sum % 2;
}

/* Ursache Andrei - 322CA */
