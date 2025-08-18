// 192.168.1.8 ip raspberrypi

#include <ifaddrs.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>



void separador(){
    char separador[] = ">-------------------------------------------------------------------<";
    printf("\n%s\n",separador);
}

void print_status(int socket_html,int socket_GET, int socket_POST,char addr[],char port_html[],char port_GET[],char port_POST[]){
    system("clear");
    separador();
    printf("Socket F.D. %d | Url GET html:\t\thttp://%s:%s\n",socket_html,addr,port_html);
    
    separador();
    printf("Socket F.D. %d | Url GET historico.txt:\thttp://%s:%s\n",socket_GET,addr,port_GET);
    
    separador();
    printf("Socket F.D. %d | Url POST mensagem:\thttp://%s:%s\n",socket_POST,addr,port_POST);
}


int get_file_bytes(FILE *txt){
    int contador=0;
    char ch;
    while ((ch = fgetc(txt)) != EOF) {
        contador++;
    }
    //printf("\n%d\n",contador);
    rewind(txt);
    return contador;
}

void send_header(int socketfd,FILE *arq){
    char header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: " ;
    //char header[] = "HTTP/1.0 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
    char request[5000];
    snprintf(request, 5000, "%s%d\r\n\r\n", header,get_file_bytes(arq));
    send(socketfd, request, strlen(request), 0);
    rewind(arq);
    return;
}

void send_file(int socketfd,char path[]){
    char buffer[1024],ch;
    int contador = 0,total = 0;
    FILE *file = fopen(path,"r");
    send_header(socketfd, file);
    rewind(file);
    while ((ch = fgetc(file))!=EOF) {
        if (contador == 1023){
            send(socketfd,buffer,contador,0);
            contador = 0;
            memset(buffer, 0, 1024);
        }
        total++;
        buffer[contador] = ch;
        contador++;        
    }
    if (contador>0){
        buffer[contador] = '\0';
        send(socketfd, buffer, contador, 0);
    }
    rewind(file);
    fclose(file);
    return;
}
void send_txt(int socketfd_GET){
    char buffer[1024],ch;
    int contador = 0;
    FILE *txt = fopen("log.txt","r");
    send_header(socketfd_GET, txt);
    while ((ch = fgetc(txt)) != EOF) {
        if (contador == 1023){
            send(socketfd_GET,buffer,contador,0);
            contador = 0;
            memset(buffer, 0, 1024);
        }

        buffer[contador] = ch;
        contador++;        
    }

    if (contador>0){
        buffer[contador] = '\0';
        send(socketfd_GET, buffer, contador, 0);
    }

    rewind(txt);
    fclose(txt);
    return;
}
/*
    
    0- usar fork() para possibilitar escuta de outros clientes
    
    1- enviar html parseado em chunks de 1024 bytes
    
    2- manter conexão com o socket para envio do historico.txt
    
    3- manter conexão com escuta constante para receber a request POST
    do javascript para q o backend consiga atualizar a lista de mensagens
                        
                                |
                                V

*/

void parse_buffer_POST(char buffer[],char **parsed_buffer){
    char temp[2000],ch;

    int contador_linhas = 0,contador=0;
    while (contador<strlen(buffer)) {
        ch = buffer[contador];
        if (buffer[contador] == '\r' && buffer[contador+1] == '\n' && buffer[contador+2] == '\r' && buffer[contador+3] == '\n' && contador_linhas == 0){
            
            contador+=4;
            contador_linhas = 1;
            continue;
        }
        else if (contador_linhas) {
            temp[contador_linhas-1] = buffer[contador];
            contador_linhas++;
        }
        
        contador++;
    }
    temp[contador_linhas-1] = '\0';
    *parsed_buffer = strdup(temp);
}

void get_username(char *parsed_buffer,char **username,char **texto){
    char *token = strtok(parsed_buffer, ":=");
    *username = strdup(token);
    token = strtok(NULL, ":=");
    *texto = token;
}

void set_conection(int socketfd_html,int socketfd_GET,int socketfd_POST,int socketfd_cliente,char addr[], char port_html[], char port_GET[],char port_POST[]){
    pid_t pid_envio_html;
    pid_envio_html = fork();
    
    if (pid_envio_html == 0){
        char buffer[5000];
        recv(socketfd_cliente,buffer,5000,0);
        send_file(socketfd_cliente,"main.html");
        shutdown(socketfd_cliente, SHUT_WR);
        close(socketfd_cliente);
        pid_t pid_GET;
        pid_GET = fork();
        if (pid_GET == 0){
            struct sockaddr_storage config_socket_client_GET;
            memset(&config_socket_client_GET,0,sizeof(config_socket_client_GET));
            socklen_t size_conf = sizeof(config_socket_client_GET);
            int socketfd_GET_client;
            socketfd_GET_client = accept(socketfd_GET, (struct sockaddr*) &config_socket_client_GET, &size_conf);
            char buffer[2000];
            
            int status_past = 1;
            int status_new = 1;
            while (status_new>0){
                status_new = recv(socketfd_GET_client, buffer, 2000, 0);
                send_txt(socketfd_GET_client);
            }
            close(socketfd_GET_client);
            exit(0);
            return;
        }
        else if(pid_envio_html == 0) {
            char buffer[2000],buffer_de_envio_ao_TxT[3000],teste[2000],*parsed_buffer,*parsed_username,*parsed_text;
            memset(buffer, 0, 2000);
            int status = 1;
            
            while (1){
                struct sockaddr_storage config_socket_client_POST;
                memset(&config_socket_client_POST,0,sizeof(config_socket_client_POST));
                socklen_t size_conf = sizeof(config_socket_client_POST);
                int socketfd_POST_client;    
                socketfd_POST_client = accept(socketfd_POST, (struct sockaddr*) &config_socket_client_POST, &size_conf);
                status = recv(socketfd_POST_client, buffer, 2000, 0);
                //printf("\nPost: %d\n",status);
                parse_buffer_POST(buffer,&parsed_buffer);
                get_username(parsed_buffer, &parsed_username, &parsed_text);
                snprintf(buffer_de_envio_ao_TxT, 3000, "[-> %s <-]=>%s\n\n", parsed_username,parsed_text);
                FILE *arquivo = fopen("log.txt", "a");
                int arquivofd = fileno(arquivo);
                flock(arquivofd, LOCK_EX);
                fputs(buffer_de_envio_ao_TxT, arquivo);                
                flock(arquivofd, LOCK_UN);
                fclose(arquivo);
                print_status(socketfd_html, socketfd_GET, socketfd_POST,addr, port_html, port_GET, port_POST);
                printf("\nstring recived: %s",buffer_de_envio_ao_TxT);
                memset(buffer,                 0, 2000);
                memset(buffer_de_envio_ao_TxT, 0, 2000);

                parsed_buffer   = NULL;
                parsed_text     = NULL;
                parsed_username = NULL;
                char resp[] = "HTTP/1.1 200 OK\r\n";
                send(socketfd_POST_client, resp, strlen(resp), 0);
                //printf("\nPost: %d",status);
                close(socketfd_POST_client);
            }
            free(parsed_buffer);
            free(parsed_text);
            free(parsed_username);
            exit(0);
            return;
        }
        exit(0);
    }
    else{
        return;
    }
}

void get_adress(char **addr,char **port_html,char **port_GET,char **port_POST){
    FILE *conf = fopen("server_adress.conf", "r");
    char temp_addr[256],temp_port_html[256],
    temp_port_GET[256] ,temp_port_POST[256];

    fgets(temp_addr     , 256, conf);
    fgets(temp_port_html, 256, conf);
    fgets(temp_port_GET , 256, conf);
    fgets(temp_port_POST, 256, conf);
    
    temp_addr[strlen(temp_addr)-1]              = '\0';
    temp_port_html[strlen(temp_port_html)-1]    = '\0';
    temp_port_GET[strlen(temp_port_GET)-1]      = '\0';
    temp_port_POST[strlen(temp_port_POST)-1]    = '\0';
    
    *addr      =         strdup(temp_addr);
    *port_html =    strdup(temp_port_html);
    *port_GET  =     strdup(temp_port_GET);
    *port_POST =    strdup(temp_port_POST);
    fclose(conf);
    return;
}

void create_socket(int *socketfd,char *addr,char *port){
    char *lista[] = {"html","GET","POST"};
    struct addrinfo socket_config;
    memset(&socket_config, 0, sizeof(socket_config));
    struct addrinfo *socket_config_list;
    int status1,status2;
    
    socket_config.ai_flags = AI_PASSIVE;
    socket_config.ai_family = AF_INET;
    socket_config.ai_socktype = SOCK_STREAM;
    status1 = getaddrinfo(addr, port, &socket_config, &socket_config_list);
    
    if (status1==-1){
        printf("Error in 'getaddrinfo()'...");
        exit(1);
    }

    *socketfd = socket(socket_config_list->ai_family,socket_config_list->ai_socktype, socket_config_list->ai_protocol);
    if (*socketfd == -1){
        close(*socketfd);
        printf("Error in 'socket()'...");
        exit(1);    
    }

    int opt = 1;
    setsockopt(*socketfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    status2 = bind(*socketfd, socket_config_list->ai_addr, socket_config_list->ai_addrlen);
    
    if (status2==-1){
        close(*socketfd);
        printf("Error in 'bind()'...");
        exit(1);
    }
    freeaddrinfo(socket_config_list);
    return;
}

int setIp(){
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            else if (strcmp(ifa->ifa_name, "wlp3s0")==0 || strcmp(ifa->ifa_name, "enp0s25")==0) {
                FILE* arq = fopen("server_adress.conf", "w");
                char buffer[5000];
                snprintf(buffer,5000,"%s\n8080\n8081\n8082\n",host);
                fputs(buffer, arq);
                fclose(arq);
                arq = fopen("main.html", "r");
                int filebytes = get_file_bytes(arq);
                char *jsbuffer1 = (char *)malloc(4);
                char *jsbuffer2 = (char *)malloc(4);
                char *jsbuffer3 = (char *)malloc(4);
                char addr1[256] = "    const URL_GET_HIST = 'http://";
                char addr2[256] = "    const URL_POST_MSG = 'http://";
                char bufferjs1[1000],bufferjs2[1000];
                char ch;
                snprintf(bufferjs1, 1000, "%s%s:8081';\n", addr1,host);
                snprintf(bufferjs2, 1000, "%s%s:8082';", addr2,host);
                int contador1 = 0,contador2 = 0,contador3 = 0,part = 1,cond = 0,cond2 = 0;
                while ((ch = getc(arq))!=EOF) {
                    if (jsbuffer1[contador1-4] == '/' && jsbuffer1[contador1-3] == '/' && jsbuffer1[contador1-2] == 'j' && jsbuffer1[contador1-1] == 's'){
                        if (cond == 0){
                            contador1++;
                            jsbuffer1 = realloc(jsbuffer1, contador1);
                            jsbuffer1[contador1-1] = '\n';
                            contador1++;
                            jsbuffer1 = realloc(jsbuffer1, contador1);
                            jsbuffer1[contador1-1] = '\0';
                        }
                        cond++;
                        continue;
                    }
                    if (!cond){
                        contador1++;
                        jsbuffer1 = realloc(jsbuffer1, contador1);
                        jsbuffer1[contador1-1] = ch;
                        continue;
                    }
                    if (cond == 1){
                        if (ch == '\n' && cond2<2){
                            cond2++;
                        }
                        else if (cond2 >= 2){
                            contador2++;
                            jsbuffer2 = realloc(jsbuffer2, contador2);
                            jsbuffer2[contador2-1] = ch;
                            continue;
                        }
                        
                    }
                    /*
                    else {
                        contador3++;
                        jsbuffer3 = realloc(jsbuffer2, contador2);
                        jsbuffer3[contador3-1] = ch;
                        if (jsbuffer3[contador3-4] == '/' && jsbuffer3[contador1-3] == '/' && jsbuffer3[contador1-2] == 'j' && jsbuffer1[contador3-1] == 's')
                            cond++;
                        continue;
                    }
                    */
                }
                contador2++;
                jsbuffer2 = realloc(jsbuffer2, contador2);
                jsbuffer2[contador2-1] = '\0';
                fclose(arq);
                arq = fopen("main.html", "w");
                fputs(jsbuffer1, arq);
                fputs(bufferjs1, arq);
                fputs(bufferjs2, arq);
                fputc('\n', arq);
                fputs(jsbuffer2, arq);
                fclose(arq);
                //free(jsbuffer3);
               // free(jsbuffer2);
                //free(jsbuffer1);
                break;
            }
        }
    }
    return 0;
}

int main (){
    setIp();
    FILE *arq = fopen("log.txt","w");
    fclose(arq);
    char *addr,*port_html,*port_GET,*port_POST;
    int socket_html,socket_GET,socket_POST;
    struct sockaddr_storage config_socket_client;
    memset(&config_socket_client,0,sizeof(config_socket_client));
    socklen_t size_conf = sizeof(config_socket_client);
    
    system("clear");
    get_adress(&addr,&port_html,&port_GET,&port_POST);

    create_socket(&socket_html, addr, port_html);

    create_socket(&socket_GET, addr, port_GET);   

    create_socket(&socket_POST, addr, port_POST);

    listen(socket_html  , 9);
    listen(socket_GET   , 9);
    listen(socket_POST  , 9);
    print_status(socket_html, socket_GET, socket_POST,addr, port_html, port_GET, port_POST);
    printf("\n[-> *placeholder* <-]=> placeholder*\n");
    while (1) {
        int socketfd_escuta = accept(socket_html,(struct sockaddr *)&config_socket_client, &size_conf);
        set_conection(socket_html, socket_GET, socket_POST,socketfd_escuta,addr,port_html,port_GET,port_POST);
    }
}
