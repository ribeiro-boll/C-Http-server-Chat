// 192.168.1.8 ip raspberrypi

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

int get_file_bytes(FILE *txt){
    int contador=0;
    char ch;
    while ((ch = fgetc(txt)) != EOF) {
        contador++;
    }
    rewind(txt);
    contador++;
    return contador;
}

void send_header(int socketfd,FILE *arq){
    char header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n" ;
    send(socketfd, header, strlen(header), 0);
    rewind(arq);
    return;
}

void send_html(int socketfd_html){
    char buffer[1024],ch;
    int contador = 0;
    FILE *html = fopen("main.html","r");
    send_header(socketfd_html, html);
    while ((ch = fgetc(html))!=EOF) {
        
        if (contador == 1023){
            send(socketfd_html,buffer,contador,0);
            contador = 0;
            memset(buffer, 0, 1024);
        }

        buffer[contador] = ch;
        contador++;        
    }

    if (contador>0){
        buffer[contador] = '\0';
        send(socketfd_html, buffer, contador, 0);
    }

    rewind(html);
    fclose(html);
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

void set_conection(int socketfd_html,int socketfd_GET,int socketfd_POST,int socketfd_cliente, int numero_user){
    pid_t pid_envio_html;
    pid_envio_html = fork();
    
    if (pid_envio_html == 0){
        char buffer[5000];
        recv(socketfd_cliente,buffer,5000,0);
        send_html(socketfd_cliente);
        close(socketfd_cliente);
        pid_t pid_GET;
        pid_GET = fork();
        if (pid_GET == 0){
            char buffer[2000];
            memset(buffer, 0, 2000);
            while (1){
                struct sockaddr_storage config_socket_client_GET;
                memset(&config_socket_client_GET,0,sizeof(config_socket_client_GET));
                socklen_t size_conf = sizeof(config_socket_client_GET);
                int socketfd_GET_client;  
                socketfd_GET_client = accept(socketfd_GET, (struct sockaddr*) &config_socket_client_GET, &size_conf);
                recv(socketfd_GET_client, buffer, 2000, 0);
                separador();
                printf("\n%s\n",buffer);
                send_txt(socketfd_GET_client);
                close(socketfd_GET_client);
            }
            return;
        }
        else {
            char buffer[2000],buffer_de_envio_ao_TxT[3000],teste[2000],*parsed_buffer,*parsed_username,*parsed_text;
            memset(buffer, 0, 2000);
            while (1){
                struct sockaddr_storage config_socket_client_POST;
                memset(&config_socket_client_POST,0,sizeof(config_socket_client_POST));
                socklen_t size_conf = sizeof(config_socket_client_POST);
                int socketfd_POST_client;    
                socketfd_POST_client = accept(socketfd_POST, (struct sockaddr*) &config_socket_client_POST, &size_conf);
                recv(socketfd_POST_client, buffer, 2000, 0);
                printf("\n%s\n",buffer);
                parse_buffer_POST(buffer,&parsed_buffer);
                get_username(parsed_buffer, &parsed_username, &parsed_text);
                snprintf(buffer_de_envio_ao_TxT, 3000, "[-> %s <-]=>%s\n\n", parsed_username,parsed_text);
                
                FILE *arquivo = fopen("log.txt", "a");
                int arquivofd = fileno(arquivo);
                flock(arquivofd, LOCK_EX);
                fputs(buffer_de_envio_ao_TxT, arquivo);                
                flock(arquivofd, LOCK_UN);
                fclose(arquivo);

                memset(buffer,                 0, 2000);
                memset(buffer_de_envio_ao_TxT, 0, 2000);

                parsed_buffer   = NULL;
                parsed_text     = NULL;
                parsed_username = NULL;
                close(socketfd_POST_client);
            }
            return;
        }
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

int main (){
    FILE *arquiv = fopen("log.txt", "w");     
    fclose(arquiv);
    char *addr,*port_html,*port_GET,*port_POST;
    int socket_html,socket_GET,socket_POST;
    int contagem_users=0;
    struct sockaddr_storage config_socket_client;
    memset(&config_socket_client,0,sizeof(config_socket_client));
    socklen_t size_conf = sizeof(config_socket_client);
    
    system("clear");
    get_adress(&addr,&port_html,&port_GET,&port_POST);
    printf("%s\n%s\n%s\n",port_html,port_GET,port_POST);
    
    separador();
    create_socket(&socket_html, addr, port_html);
    printf("Socket F.D. %d | Url GET html:\t\thttp://%s:%s\n",socket_html,addr,port_html);
    
    separador();
    create_socket(&socket_GET, addr, port_GET);
    printf("Socket F.D. %d | Url GET historico.txt:\thttp://%s:%s\n",socket_GET,addr,port_GET);
    
    separador();
    create_socket(&socket_POST, addr, port_POST);
    printf("Socket F.D. %d | Url POST mensagem:\thttp://%s:%s\n",socket_POST,addr,port_POST);
    
    printf("\n\n");
    listen(socket_html  , 9);
    listen(socket_GET   , 9);
    listen(socket_POST  , 9);
    FILE *arquivo = fopen("contagem.conf", "w");
    fputc('0', arquivo);
    rewind(arquivo);
    fclose(arquivo);
    
    while (1) {
        int socketfd_escuta = accept(socket_html,(struct sockaddr *)&config_socket_client, &size_conf);
        contagem_users++;
        

        set_conection(socket_html, socket_GET, socket_POST,socketfd_escuta,contagem_users);
    }
}
