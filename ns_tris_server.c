#include <sys/select.h> 
#include <sys/time.h> 

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include<stdio.h>
#include<stdlib.h>

#define MAX_NUM_CONN_PENDENTI 10
#define DIM_COMANDO 21

fd_set set_lista;
unsigned int num_utenti;
int porta_server;

struct elem_utente
{
	char *nome, stato; /* stato, libero o occupato */
	int porta_udp, socket_tcp;
	unsigned long indirizzo;
	struct elem_utente *next, *avversario;
};
struct elem_utente *lista_utenti=NULL;
void inserisci_elemento(struct elem_utente *nuovo)
{
	nuovo->next=lista_utenti;
	lista_utenti=nuovo;
	num_utenti+=1;
}
struct elem_utente * cerca(char *nom)
{
	struct elem_utente *temp=lista_utenti;
	while (temp)
	{
		if (!strcmp(nom, temp->nome))
			return temp;
		temp=temp->next;
	}
	return NULL;
}
struct elem_utente *trova_client(int socket_client) /* ritorna NULL non ha trovato nessun client */
{
	struct elem_utente *scorri=lista_utenti;
	while (scorri)
	{
		if (scorri->socket_tcp == socket_client)
			return scorri;
		scorri=scorri->next;
	}
	return NULL;
}
void rimuovi_elemento(struct elem_utente *rim)
{
	struct elem_utente *scorri=lista_utenti;
	if (!lista_utenti)
		return ; /* lista vuota */
	if (lista_utenti == rim)
	{
		lista_utenti=lista_utenti->next;
		num_utenti-=1;
		/* ultima_connessione=lista_utenti->socket_tcp;  perchè è l'ultimo client che si è connesso ed è ancora attivo */
		return ;
	}
	while (scorri->next!=rim && scorri->next!=NULL)
		scorri=scorri->next;
	if (!scorri->next)
		return ; /* elemento non trovato */
	scorri->next=(scorri->next)->next;
	num_utenti-=1;
}
void who(struct elem_utente *client)
{
	int len_nomi;
	struct elem_utente *scorri=lista_utenti;
	len_nomi=num_utenti-1; /* togliamo quello che sta effettuando la richiesta */
	len_nomi=send(client->socket_tcp, (void *) &len_nomi, sizeof(len_nomi), 0);
	if (len_nomi==-1 || len_nomi<sizeof(num_utenti))
    {
       perror("Errore who 0.\n");
       exit(1);
    }
	
	while (scorri)
	{
		if (scorri != client)
		{
			len_nomi=strlen(scorri->nome);
			len_nomi=send(client->socket_tcp, (void *) &len_nomi, sizeof(len_nomi), 0);
        	if (len_nomi==-1 || len_nomi<sizeof(len_nomi))
       		{
        	   perror("Errore who 1.\n");
        	   exit(1);
        	}
        	len_nomi=send(client->socket_tcp, (void *) scorri->nome, strlen(scorri->nome), 0);
        	if (len_nomi==-1 || len_nomi<strlen(scorri->nome))
        	{
        	   perror("Errore who 2.\n");
        	   exit(1);
        	}
        	len_nomi=send(client->socket_tcp, (void *) &(scorri->stato), sizeof(char), 0);
        	if (len_nomi==-1 || len_nomi<sizeof(char))
        	{
        	   perror("Errore who 3.\n");
        	   exit(1);
        	}
        }
        scorri=scorri->next;
	}
}
void connect_client(struct elem_utente *client)
{
	char nome[ DIM_COMANDO ], risposta;
	int err, dim_nome;
	struct elem_utente *avversario=NULL;
	
	err=recv(client->socket_tcp, (void *) &dim_nome, sizeof(dim_nome), 0); /* riceve la dimensione del nome di chi si vuole sfidare */
	if (err<0)
	{
	    printf("Errore in \"connect_client\" lettura dim_nome.\n");
    	exit(1);
   	}
   	err=recv(client->socket_tcp, (void *) nome, dim_nome, 0); /* riceve il nome di chi si vuole sfidare */
	if (err<0)
	{
	    printf("Errore in \"connect_client\" lettura nome.\n");
    	exit(1);
   	}
	
   	nome[dim_nome]=0;
   	avversario=cerca(nome);
   	if (!avversario) /* il nome cercato non esiste */
   	{
   		printf("Avversario inesistente.\n");
   		risposta='i';
   		err=send(client->socket_tcp, (void *) &risposta, sizeof(risposta), 0); /* invia risposta al client */
        if (err<0)
        {
           perror("Errore connect_client risposta avversario inesistente.\n");
           exit(1);
        }
        return ;
   	}
	
	if (avversario->stato!='l') /* l'avversario è occupato e il server ne avvisa il richiedente */
	{
		risposta='o';
		err=send(client->socket_tcp, (void *) &risposta, sizeof(char), 0);
    	if (err<0)
    	{
    	   perror("Errore nella \"lettura_server()\" default 1,5.\n");
    	   exit(1);
    	}
		return ;
	}
   	/* altrimenti, invia all'avversiario la richiesta di gioco e i dati di chi lo vuole sfidare */
   	risposta='r';
   	err=send(avversario->socket_tcp, (void *) &risposta, sizeof(risposta), 0);
    if (err<0)
    {
       perror("Errore connect_client richiesta all'avversario 1.\n");
       exit(1);
    }
    err=strlen(client->nome);
    err=send(avversario->socket_tcp, (void *) &err, sizeof(err), 0); /* invia la len del nome del richiedente client */
    if (err<0)
    {
       perror("Errore connect_client richiesta all'avversario 2.\n");
       exit(1);
    }
    err=send(avversario->socket_tcp, (void *) client->nome, strlen(client->nome), 0); /* invia il nome del richiedente */
    if (err<0)
    {
       perror("Errore connect_client richiesta all'avversario 3.\n");
       exit(1);
    }
    err=send(avversario->socket_tcp, (void *) &(client->porta_udp), sizeof(int), 0); /* invia la porta udp del richiedente */
    if (err<0)
    {
       perror("Errore connect_client richiesta all'avversario 3.\n");
       exit(1);
    }
    err=send(avversario->socket_tcp, (void *) &(client->indirizzo), sizeof(client->indirizzo), 0); /* invia l'indirizzo del richiedente */
    if (err<0)
    {
       perror("Errore connect_client richiesta all'avversario 3.\n");
       exit(1);
    }
    /* la funzione non termina qui perchè il client risponde subito quindi non ci sono attese */
    err=recv(avversario->socket_tcp, (void *) &risposta, sizeof(risposta), 0);
	if (err<0)
	{
	    printf("Errore in \"connect_client\" lettura risposta dell'avversario 4.\n");
    	exit(1);
   	}
	/* l'avversario ha accettato, è occupato o ha rifiutato, cmq sia ne informiamo il richiedente */
   	err=send(client->socket_tcp, (void *) &risposta, sizeof(risposta), 0); /* invia risposta al client */
    if (err<0)
    {
       perror("Errore connect_client richiesta all'avversario 5.\n");
       exit(1);
    }
    /* Se la sfida è stata accettata mandiamo la porta udp del richiesto al richiedente */
    if (risposta=='a')
    {
    	err=send(client->socket_tcp, (void *) &(avversario->porta_udp), sizeof(int), 0); /* invia la porta udp dell'avversario al client */
    	if (err<0)
    	{
    	   perror("Errore connect_client richiesta all'avversario 6.\n");
    	   exit(1);
    	}
    	err=send(client->socket_tcp, (void *) &(avversario->indirizzo), sizeof(avversario->indirizzo), 0); /* invia l'indirizzo dell'avversario al richiedente */
    	if (err<0)
    	{
    	   perror("Errore connect_client richiesta all'avversario 7.\n");
    	   exit(1);
    	}
    	client->avversario=avversario;
    	avversario->avversario=client;
    	client->stato=avversario->stato='o'; /* ora sono entrambi occupati in una partita */
    	printf("%s si è connesso a %s\n", client->nome, avversario->nome);
    }
}
void disconnect(struct elem_utente *client)
{
	if (client->avversario == NULL)
	{
		printf("--- Errore nella disconnect.---\n");
		return ;
	}
	if (client->avversario != (struct elem_utente *) 0xFFFFFFFF)
	{
		printf("%s si è disconnesso da %s\n", client->nome, client->avversario->nome);
		(client->avversario)->avversario = (struct elem_utente *) 0xFFFFFFFF;
	}
	client->avversario=NULL;
	client->stato='l';
	printf("%s è libero\n", client->nome);
}
void quit(struct elem_utente *client)
{
printf("Quit di %s.\n", client->nome);
fflush(stdout);
	close(client->socket_tcp);/*se no quando il client fa quit dopo disconnect e la connessione è ancora stabilita il server sbrocca con infinite richieste 'd' */
	rimuovi_elemento(client);
	FD_CLR(client->socket_tcp, &set_lista);
	free(client->nome);
	free(client);
}
void gestisci_client(struct elem_utente *client)
{
	int err;
	char comando='x';
	
	if (!client)
	{
		printf("Errore, client inesistente! \"gestisci_client\"\n");
		return ;
	}
/*#*/printf("Client gestito: %s\n", client->nome);
	
	/* lunghezza del comando, char */
	err=recv(client->socket_tcp, (void *) &comando, sizeof(comando), 0);
	if (0 == err) /* il client ha usato la "quit" ed ha quindi chiuso la connessione */
	{
		printf("Client %s terminato!\n", client->nome);
		quit(client);
		return ;
	}
	if (err<0)
   	{
   	   printf("Errore in \"gestisci client\" lettura comando.\n");
   	   exit(1);
   	}
    switch (comando)
    {
        case 'w': /* !who */
        {
             who(client);
             break;
        }
        case 'c': /* !connect */
        {
             connect_client(client);
             break;
        }
        case 'd': /* !disconnect */
        {
             disconnect(client);
             break;
        }
    	default:
    	{
    		printf("Comando non riconosciuto!\n");
    		return ;
    	}
    }
/*#*/printf("Comando %c completato.\n", comando);
}
void aggiungi_client(int socket)
{
	char c;
	int len_nome, err;
	struct sockaddr_in indirizzo_client;
	struct elem_utente *nuovo=malloc(sizeof(struct elem_utente));
	
	err=sizeof(indirizzo_client);
	memset(&indirizzo_client, 0, err);
	getpeername(socket, (struct sockaddr *)&indirizzo_client, (socklen_t *) &err); /* leggiamo l'indirizzo IP di chi si è connesso a noi */
	
	nuovo->indirizzo=(indirizzo_client.sin_addr).s_addr;
	nuovo->stato='l';
	nuovo->socket_tcp=socket;
	nuovo->avversario=NULL;
	/* lunghezza del nome, int , in formato di rete */
	err=recv(socket, (void *) &len_nome, sizeof(len_nome), 0);
	if (err<0)
    {
       printf("Errore in \"aggiungi client\" lettura lunghezza del nome.\n");
       exit(1);
    }
	len_nome=ntohl(len_nome);
	nuovo->nome=malloc(len_nome+1);
	err=recv(socket, (void *) (nuovo->nome), len_nome, 0);
	if (err<0)
    {
       printf("Errore in \"aggiungi client\" lettura del nome.\n");
       exit(1);
    }
    nuovo->nome[len_nome]=0;
	/* porta udp, short, formato rete */
	err=recv(socket, (void *) &(nuovo->porta_udp), sizeof(short int), 0);
	if (err<0)
    {
       printf("Errore in \"aggiungi client\" lettura porta udp.\n");
       exit(1);
    }
	nuovo->porta_udp=ntohs(nuovo->porta_udp);
	if (!cerca(nuovo->nome)) /* se non ci sono gia altri client con lo stesso nome */
	{
		c='z'; /* un carattere a caso, tanto non verrà controllato dal client */
		err=send(socket, (void *) &c, sizeof(c), 0);
    	if (err<0)
    	{
    	   perror("Errore in \"aggiungi client\" connessione accettata.\n");
    	   exit(1);
    	}
		inserisci_elemento(nuovo); /* inserisce l'elemento appena creato */
	}
	else /* comunica al client la fallita connesione */
	{
		c='e';
		err=send(socket, (void *) &c, sizeof(c), 0);
    	if (err<0)
    	{
    	   perror("Errore in \"aggiungi client\" connessione fallita.\n");
    	   exit(1);
    	}
    	quit(nuovo);
    }
	printf("Connessione stabilita con il client\n%s si e' connesso\n%s e' libero\n", nuovo->nome, nuovo->nome);
}

int main (int argc, char** argv)
{
	int err, len_struttura_sk_ad_in, socket_server, socket_client, ultima_connessione, i;
	struct sockaddr_in addr_server, addr_client;
	fd_set set_temp;

	if (argc != 3)
	{
       printf("Numero argomenti errato.\nUsa %s <host> <porta>\n", argv[0]);
       exit(1);
    }
    porta_server=atoi(argv[2]);
    FD_ZERO(&set_lista);
    printf("Indirizzo: %s (Porta: %d)\n", argv[1], porta_server);
    
    socket_server = socket(AF_INET, SOCK_STREAM, 0);    
    /* preparazione della struttura del server */
	memset(&addr_server, 0, sizeof(addr_server));
	addr_server.sin_family = AF_INET;
	addr_server.sin_port = htons(porta_server);
	if (!inet_pton(AF_INET, argv[1], &addr_server.sin_addr.s_addr))
    {
       printf("Indirizzo server non valido!\n");
       exit(1);
    }
	/*addr_server.sin_addr.s_addr = htonl(INADDR_ANY);*/
	err = bind(socket_server, (struct sockaddr *) &addr_server, sizeof(addr_server));
	if (err < 0)
	{
       printf("Errore nella bind: %i\n", err);
       exit(1);
    }
	err = listen(socket_server, MAX_NUM_CONN_PENDENTI);
	if (err < 0)
	{
       printf("Errore nella listen: %i\n", err);
       exit(1);
    }
    
    FD_ZERO(&set_lista);
    FD_SET(socket_server, &set_lista);
    ultima_connessione=socket_server;
	
	num_utenti=0;
	
	while (1)
	{
		set_temp=set_lista;
        switch (select(ultima_connessione+1, &set_temp, NULL, NULL, NULL))
        {
               case -1:
               {
                    perror("Errore nella select(): ");
                    exit(1);
               }
               case 0:
               {
                    printf("Case 0 dello switch.\n");
                    break;
               }
               default:
               {
                       for (i=0; i<=ultima_connessione+1; i++)
                       {
                           if (FD_ISSET(i, &set_temp))
                           {
                              if (i == socket_server)
                              {
                              	/* attesa dei client */
								len_struttura_sk_ad_in = sizeof(addr_client);
								socket_client = accept(socket_server, (struct sockaddr *) &addr_client, (socklen_t *) &len_struttura_sk_ad_in);
    							FD_SET(socket_client, &set_lista); /* per ogni client che si connette */
    							ultima_connessione=socket_client;
    							aggiungi_client(socket_client); /* memorizza il nuovo client */
    						  }
                              else
                              	gestisci_client(trova_client(i));
                           } /* fine if */
                       } /* fine for */
               } /* fine default */
        } /* fine switch */
    } /* fine while */
    
	return 0;
}
