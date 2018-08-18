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
#define DIM_NOME_UTENTE 21
#define MAX_DIM_COMANDI 12
#define NUM_COMANDI 7
#define HELP "Sono disponibili i seguenti comandi:\n*  !help  -->  mostra  l'elenco  dei  comandi  disponibili\n*  !who  -->  mostra  l'elenco  dei  client  connessi  al  server\n*  !connect  nome_client  -->  avvia  una  partita  con  l'utente  nome_client\n*  !disconnect  -->  disconnette  il  client  dall'attuale  partita  intrapresa con un altro peer\n*  !quit  -->  disconnette  il  client  dal  server\n*  !show_map  -->  mostra  la  mappa  di gioco\n*  !hit  num_cell  -->  marca  la  casella  num_cell  (valido  solo  quando  e' il proprio turno)\n"

char comandi[][ MAX_DIM_COMANDI ]={"!help",
                   					"!who",
                   					"!connect",
                   					"!disconnect",
                   					"!quit",
                   					"!show_map",
                   					"!hit"};
char campo[3][3]={"---",
                  "---",
                  "---"};
int porta_udp, socket_server, socket_udp, udp_avversario; /* port_udp e udp_avversario sono nel formato host */
struct sockaddr_in indirizzo_avversario;
char nome_utente[ DIM_NOME_UTENTE ], nome_avversario[ DIM_NOME_UTENTE ], stato, shell, segno_avversario, segno_mio;
fd_set set_lista, set_temp;

int ricevuta_hit;
char turno; /* turno==segno_mio -> mio turno, altrimenti turno avversario */

int riconosci(char *s)
{
     int i;
     for (i=0; i< NUM_COMANDI ; i++)
          if (!strcmp(comandi[i], s))
             return i;
     return -1;
}
void apri_connessione_server(char* nome_server, int porta_server)
{
     unsigned short int p;
     struct sockaddr_in addr_server;
     
     if (porta_server<0 || porta_server>65535)
     {
        printf("Porta non valida: %d\n", porta_server);
        exit(1);
     }
     p=porta_server;
     
     if ((socket_server=socket(AF_INET, SOCK_STREAM, 0)) == -1)
     {
        perror("Errore nella \"socket()\": ");
        exit(1);
     }
     
     memset((char *) &addr_server, 0, sizeof(addr_server));
     addr_server.sin_family = AF_INET;
     addr_server.sin_port = htons(p);
     if (!inet_pton(AF_INET, nome_server, &addr_server.sin_addr.s_addr))
     {
        printf("Indirizzo server non valido!\n");
        exit(1);
     }
     
     if (connect(socket_server, ( struct sockaddr *) &addr_server, sizeof(addr_server)))
     {
        perror("Errore nella \"connect()\": ");
        exit(1);
     }
}
void who() /* parte A */
{
     int err;
     /*if (stato != 'l')
     {
     	printf("Sei gia occupato in un altra attività!\n");
     	return ;
     }*/
     stato='w';
     err = send(socket_server, (void *) &stato, sizeof(char), 0);
     if (err == -1 || err < sizeof(char))
     {
        perror("Errore nella \"send()\" (comando who): ");
        exit(1);
     }
}
void connect_client() /* parte A */
{
     char c='c';
     int err;
     if (stato == 'o')
     {
     	printf("Sei gia connesso con un giocatore!\n");
     	return ;
     }
     if (stato != 'l')
     {
     	printf("Sei gia occupato in un altra attivita!\n");
     	return ;
     }
     if (!strcmp(nome_avversario, nome_utente))
     {
     	printf("Non puoi connetterti con te stesso!\n");
     	return ;
     }
     err = send(socket_server, (void *) &c, sizeof(c), 0);
     if (err<0)
     {
        perror("Errore richiesta 1: ");
        exit(1);
     }
     err=strlen(nome_avversario);
     err=send(socket_server, (void *) &err, sizeof(err), 0);
     if (err==-1 || err<sizeof(err))
     {
        perror("Errore richiesta 2: ");
        exit(1);
     }
     err=send(socket_server, (void *) nome_avversario, strlen(nome_avversario), 0);
     if (err==-1 || err<strlen(nome_avversario))
     {
        perror("Errore richiesta 3: ");
        exit(1);
     }
     stato=c;
}
void disconnect()
{
     char d='d'; /* client disconnesso */
     int err;
     if (shell != '#')
     {
        printf("Nessuna partita in corso!\n");
        return ;
     }
     err = send(socket_server, (void *) &d, sizeof(char), 0);
     if (err==-1 || err<sizeof(char))
     {
        perror("Errore nella \"send()\" (comando disconnect): ");
        exit(1);
     }
     /* invia all'altro client su UDP la disconnessione */
     err=sendto(socket_udp, (const void *) &d, sizeof(d), 0, (struct sockaddr *) &indirizzo_avversario, (socklen_t) sizeof(indirizzo_avversario));
     if (err<0)
     {
     	printf("Errore nella \"disconnect\" in sendto.\n");
     	exit(1);
     }
     memset(&indirizzo_avversario, 0, sizeof(indirizzo_avversario));
     shell='>';
     stato='l'; /* nessuno stato presente - libero */
     printf("Disconnessione  avvenuta  con  successo:  TI  SEI  ARRESO\n");
}
void quit()
{
     close(socket_server);
     close(socket_udp);
     printf("Client  disconnesso  correttamente\n");
     exit(0);
}
void reset_campo() /* da chiamare alla fine/inizio di ogni partita */
{
	int i, j;
	for (i=0;i<3;i++)
		for (j=0;j<3;j++)
			campo[i][j]='-';
}
void show_map() /* fatta */
{
     int i, j;
     for (i=0; i<3; i++)
     {
         for (j=0; j<3; j++)
             printf("%c ", campo[i][j]);
         printf("\n");
     }
}
char vittoria(char simbolo) /* restituisce 1 se il giocatore con simbolo "simbolo" ha vinto, 0 altrimenti */
{ /* il parametro serve per discriminare il caso, sempre verificati a inizio partita, che ci siano tre simboli "-" allineati */
     int i;
     for (i=0; i<3; i++)
         if ( (campo[i][0]==campo[i][1] && campo[i][0]==campo[i][2] && campo[i][0]==simbolo) || (campo[0][i]==campo[1][i] && campo[0][i]==campo[2][i] && campo[0][i]==simbolo) )
            return 1;
     if ( ((campo[0][0]==campo[1][1] && campo[0][0]==campo[2][2]) || (campo[0][2]==campo[1][1] && campo[0][2]==campo[2][0])) && campo[1][1]==simbolo )
        return 1;
     return 0;
}
int segna(char bersaglio, char segno) /* "bersaglio" va da 0 a 9 secondo l'ordine del tastierino numerico */
{
	int x;
	bersaglio--;
	if (bersaglio < 3)
		x=2;
	else
		if (bersaglio > 5)
		{
			x=0;
			bersaglio-=6;
		}
		else
		{
			x=1;
			bersaglio-=3;
		}
	if (campo[x][(int) bersaglio] != '-')
		return 0;
	campo[x][(int) bersaglio]=segno;
	return 1;
}
void hit(char c)
{
     int err;
     if (shell != '#')
     {
        printf("Nessuna partita in corso!\n");
        return ;
     }
     if (turno != segno_mio)
     {
     	printf("Aspetta il tuo turno.\n");
     	return ;
     }
     if (!segna(c, segno_mio)) /* inserisco la coordinata nel campo */
     {
     	printf("La coordinata inserita e' gia occupata. Ritenta.\n");
     	return ;
     }
     turno=segno_avversario;
     /* invia la coordinata all'altro giocatore, tramite UDP */
     err=sendto(socket_udp, (const void *) &c, sizeof(c), 0, (struct sockaddr *) &indirizzo_avversario, (socklen_t) sizeof(indirizzo_avversario));
     if (err<0)
     {
     	printf("Errore nella \"hit\" sendto().\n");
     	exit(1);
     }
     if (vittoria(segno_mio))
     {
        stato='l';
        shell='>';
        memset(&indirizzo_avversario, 0, sizeof(indirizzo_avversario)); /* azzera indirizzo_avversario */
        /* deve comunicare al server che è libero, perchè è finita la partita! */
        c='d';
        err = send(socket_server, (void *) &c, sizeof(char), 0);
     	if (err==-1 || err<sizeof(char))
     	{
     	   perror("Errore nella \"send()\" (comando disconnect): ");
     	   exit(1);
     	}
        printf("Hai vinto!!!\n");
     }
}
void invia_dati_client()
{
     unsigned short int porta;
     int len, err;
     char c;
     len = strlen((const char *) nome_utente);
     err = htonl((unsigned int) len);
     err = send(socket_server, (void *) &err, sizeof(int), 0);
     if (err == -1 || err < sizeof(int))
     {
        perror("Errore nella \"send()\" (byte nome utente): ");
        exit(1);
     }
     err = send(socket_server, (void *) nome_utente, len, 0);
     if (err == -1 || err < len)
     {
        perror("Errore nella \"send()\" (nome utente): ");
        exit(1);
     }
     porta = htons(porta_udp);
     err = send(socket_server, (void *) &porta, sizeof(porta), 0);
     if (err == -1 || err < sizeof(porta))
     {
        perror("Errore nella \"send()\" (porta udp): ");
        exit(1);
     }
     /* legge la risposta del server */
     err=recv(socket_server, (void *) &c, sizeof(c), 0);
	 if (err<0)
     {
        printf("Errore nella \"send()\" lettura risposta server.\n");
		exit(1);
	 }
	 if (c == 'e') /* e se il nome inciato esisteva gia termina */
	 {
	 	printf("Nome gia esistente, cambia nome.\n");
	 	exit(2);
	 }
}
void lettura_tastiera()
{
     char coordinata, buf[ MAX_DIM_COMANDI ];
     scanf("%s", buf);
     switch(riconosci(buf))
	 {
      case 0: /* !help */
      {
           printf("%s\n", HELP);
           break;
      }
      case 1: /* !who */
      {
           who();
           break;
      }
      case 2: /* !connect */
      {
           memset(nome_avversario, 0, DIM_NOME_UTENTE );
           scanf("%s", nome_avversario);
           printf("Inizio connessione con %s\n", nome_avversario);
           connect_client(nome_avversario);
           break;
      }
      case 3: /* !disconnect */
      {
           disconnect();
           break;
      }
      case 4: /* !quit */
      {
           quit();
           break;
      }
      case 5: /* !show_map */
      {
           show_map();
           break;
      }
      case 6: /* !hit */
      {
           scanf(" %c", &coordinata);
           if (coordinata<48 || coordinata>57)
           {
              printf("Coordinata errata. Ritenta.\n");
              return ;
           }
           hit(coordinata-48);
           break;
      }
      default: printf("\nComando non riconosciuto.\n");
     }
}
void lettura_server()
{
     int i;
     int len_nome, ret;
     unsigned int num_client;
     char risposta;
     char nome_client[ DIM_NOME_UTENTE ];
     switch (stato)
     {
            case 'w': /* comando !who  parte B */
            {
     			 printf("Client connessi al server:\n");
                 /* lettura di un num_client */
                 ret=recv(socket_server, (void *) &num_client, sizeof(num_client), 0);
				 if (ret<0)
			     {
			        printf("Errore in \"lettura_server\" lettura num_client.\n");
       				exit(1);
   				 }
                 for (i=0; i<num_client; i++)
                 {
                     memset(nome_client, 0, DIM_NOME_UTENTE ); /* azzera nome_client */
                     /* lettura di len_nome */
                     ret=recv(socket_server, (void *) &len_nome, sizeof(len_nome), 0);
				 	 if (ret<0)
			    	 {
			   		    printf("Errore in \"lettura_server\" lettura len_nome.\n");
       					exit(1);
   				 	 }
                     if (len_nome >= DIM_NOME_UTENTE) /* controllo inutile */
                     {
                        printf("\nErrore: nome client connesso al server troppo lungo!\n");
                        exit(1);
                     }
                     ret=recv(socket_server, (void *) nome_client, len_nome, 0);
				 	 if (ret<0)
			    	 {
			   		    printf("Errore in \"lettura_server\" lettura nome_client.\n");
       					exit(1);
   				 	 }
   				 	 ret=recv(socket_server, (void *) &risposta, sizeof(risposta), 0);
				 	 if (ret<0)
			    	 {
			   		    printf("Errore in \"lettura_server\" lettura stato client.\n");
       					exit(1);
   				 	 }
                     nome_client[len_nome]='\0';
                     printf("%d. %s\t%s\n", (i+1), nome_client, ((risposta=='l')?"libero":"occupato"));
                 }
                 printf("\n");
                 stato=(shell=='#'?'o':'l'); /* per poter eseguire il comando who durante una partita */
                 break;
            }
            case 'c': /* comando !connect parte B */
            {
            	 ret = recv(socket_server, & risposta, sizeof(risposta), 0);
                 if (ret<0)
                 { /* lettura della risposta */
                     perror("Errore nella \"lettura_server()\" (case c, lettura risposta): ");
                     exit(1);
                 }
                 stato='l';
                 switch (risposta)
                 {
                        case 'a': /* accettato */
                        {
                             printf("%s ha accettato la partita\npartita avviata con %s\nil tuo simbolo e': X\nE' il tuo turno:\n", nome_avversario, nome_avversario);
                             segno_mio='X';
                             segno_avversario='O';
                             turno=segno_mio;
                             stato='o'; /* client occupato in una partita */
                             shell='#'; /* la schell indica lo stato di gioco */
                             ret = recv(socket_server, &udp_avversario, sizeof(udp_avversario), 0);
                             if (ret==-1)
                             { /* lettura della porta del client in un intero */
                                perror("Errore nella \"lettura_server()\" (case c, lettura porta client): ");
                                exit(1);
                             }
                             
                             memset(&indirizzo_avversario, 0, sizeof(indirizzo_avversario));
                			 indirizzo_avversario.sin_family=AF_INET;
    					   	 indirizzo_avversario.sin_port=htons(udp_avversario);
    					   	 /* ricezione dell' indirizzo ip dell'avversario */
    						 ret = recv(socket_server, &((indirizzo_avversario.sin_addr).s_addr), sizeof(unsigned long), 0);
                			 if (ret<0)
                			 {
                	    		 perror("Errore nella \"lettura_server()\" default 3,5.\n");
                	    		 exit(1);
                			 }
                             reset_campo(); /* azzera il campo per la nuova partita */
                             break;
                        }
                        case 'i': /* inesistente */
                        {
                             printf("Impossibile  connettersi  a  %s:  utente  inesistente.\n", nome_avversario);
                             break;
                        }
                        case 'o': /* occupato */
                        { /* non si fa distinzione tra la 'o' inviata dal server e quella inviata dal client perchè sta eseguendo una who o connect */
                             printf("Impossibile  connettersi  a  %s: l'utente e' gia' occupato in un altra partita.\n", nome_avversario);
                             break;
                        }
                        case 'r': /* richiesta rifiutata dall'avversario */
                        {
                        	printf("Impossibile  connettersi  a  %s:  l'utente  ha  rifiutato  la  partita.\n", nome_avversario);
                        	break ;
                        }
                        default: { printf("Risposta non comprensibile.\n"); stato='l'; }
                 } /* fine switch interno */
                 break;
            } /* fine case 'c' */
            default: /* forse mi stanno chiedendo di giocare */
            {
            	ret = recv(socket_server, &risposta, sizeof(risposta), 0);
            	if (0 == ret) /* il server ha chiuso la connessione */
				{
					printf("Il server ha chiuso la connessione!\n");
					fflush(stdout);
					exit(2);
				}
				else
                	if (ret<0)
                	{ /* lettura della risposta */
                	    perror("Errore nella \"lettura_server()\" default 1.\n");
                	    exit(1);
                	}
                if (risposta=='r') /* è una richiesta di gioco */
                {
                	ret = recv(socket_server, &len_nome, sizeof(len_nome), 0); /* ricevo lunghezza nome di chi richiede di giocare */
                	if (ret<0)
                	{
                	    perror("Errore nella \"lettura_server()\" default 2\n.");
                	    exit(1);
                	}
                	memset(nome_avversario, 0, DIM_NOME_UTENTE );
                	ret = recv(socket_server, nome_avversario, len_nome, 0); /* ricezione del nome suddetto */
                	if (ret<0)
                	{
                	    perror("Errore nella \"lettura_server()\" default 3.\n");
                	    exit(1);
                	}
                	nome_avversario[len_nome]=0;
                	
                	if (stato!='l') /* se sono gia occupato con una who o una connect */
                	{
                		/* leggo cio che rimane da leggere dal server */
                		ret = recv(socket_server, nome_client, (sizeof(udp_avversario)+sizeof(unsigned long)), 0); /* ricezione della porta udp dell'avversario */
                		if (ret<0)
                		{
                	    	perror("Errore nella \"lettura_server()\" rifiuto richiesta di connessione.\n");
                	    	exit(1);
                		}
                		/* poi gli dico che sono gia occupato a fare qualcosaltro */
                		risposta='o';
                		ret=send(socket_server, (void *) &stato, sizeof(char), 0);
    					if (ret<0)
    					{
    					   perror("Errore nella \"lettura_server()\" default 1,5.\n");
    					   exit(1);
    					}
    					return ;
                	}
                	else
                	{
                		ret = recv(socket_server, &udp_avversario, sizeof(udp_avversario), 0); /* ricezione della porta udp dell'avversario */
                		if (ret<0)
                		{
                	    	perror("Errore nella \"lettura_server()\" default 3,5.\n");
                	    	exit(1);
                		}
                		memset(&indirizzo_avversario, 0, sizeof(indirizzo_avversario));
                		indirizzo_avversario.sin_family=AF_INET;
    					indirizzo_avversario.sin_port=htons(udp_avversario);
    					/* ricezione dell'indirizzo ip dell'avversario */
                		ret = recv(socket_server, &((indirizzo_avversario.sin_addr).s_addr), sizeof(unsigned long), 0);
                		if (ret<0)
                		{
                		    perror("Errore nella \"lettura_server()\" default 3,5.\n");
                		    exit(1);
                		}
                	}
                	
                	printf("Vuoi giocare con %s ? [y, n]\t", nome_avversario);
                	scanf(" %c", &risposta);
                	if (risposta=='y' || risposta=='Y')
                	{
                		printf("Il tuo simbolo e' O.\n");
                		risposta='a'; /* ho accetato la partita */
                		ret=send(socket_server, (void *) &risposta, sizeof(risposta), 0);
    					if (ret<0)
    					{
    					   perror("Errore nella \"lettura_server()\" default 4.\n");
    					   exit(1);
    					}
    					segno_mio='O';
                        segno_avversario='X';
                        turno=segno_avversario;
                        stato='o'; /* client occupato in una partita */
                        shell='#'; /* la schell indica lo stato di gioco */
                        reset_campo(); /* azzera il campo per la nuova partita */
                	}
                	else
                	{
                		printf("Hai declinato.\n");
                		risposta='r'; /* ho rifiutato la partita */
                		ret=send(socket_server, (void *) &risposta, sizeof(risposta), 0);
    					if (ret<0)
    					{
    					   perror("Errore nella \"lettura_server()\" default 5.\n");
    					   exit(1);
    					}
                	}
                }
            }
     } /* fine switch esterno */
}
void lettura_udp()
{
     struct sockaddr_in ric; 
     char val;
     int err, len_ric = sizeof(ric);
     err = recvfrom(socket_udp, &val, sizeof(val), 0, (struct sockaddr*)&ric, (socklen_t *) &len_ric);
     if (err<0)
     {
        perror("recvfrom lettura_udp(): "); 
        exit(1); 
     }
     if (!err)
     {
        printf("Ricevuti 0 byte in lettura_udp.\n");
        return ;
     }
     if ((ric.sin_addr.s_addr) != (indirizzo_avversario.sin_addr.s_addr))
     {
        printf("Ricevuto datagram da un pc non avversario.\n");
        return ;
     }
     
     if (val=='d') /* l'avversario si è disconnesso e arreso */
     {
        shell='>';
        stato='l';
        memset(&indirizzo_avversario, 0, sizeof(indirizzo_avversario)); /* azzera indirizzo_avversario */
        /* comunico al server cher sono libero */
        err=send(socket_server, (void *) &val, sizeof(val), 0);
    	if (err<0)
    	{
    	   perror("Errore nella \"lettura_udp()\" val=='d'.\n");
    	   exit(1);
    	}
        printf("L'avversario si e' arreso.\tHai vinto!\n");
        return ;
     }
     
     if (val>0 && val<10) /* l'avversario ha segnato un colpo - da 1 a 9 */
     {
        if (turno != segno_avversario) /* non si dovrebbe mai verificare */
     	{
     		printf("L'avversario non ha aspettato il suo turno.\n");
     		return ;
     	}
     	turno=segno_mio;
        segna(val, segno_avversario);
        if (vittoria(segno_avversario)) /* se l'avversorio ha vinto */
        {
           stato='l';
           shell='>';
           memset(&indirizzo_avversario, 0, sizeof(indirizzo_avversario)); /* azzera indirizzo_avversario */
           /* deve comunicare al server che è libero, perchè è finita la partita! */
           val='d';
           err = send(socket_server, (void *) &val, sizeof(char), 0);
           if (err==-1 || err<sizeof(char))
           {
           		perror("Errore nella \"send()\" (comando disconnect): ");
           		exit(1);
           }
           printf("Hai perso!\n");
        }
        else
        	ricevuta_hit=1;
     }
}
int main(int argc, char**argv)
{
	
	struct sockaddr_in mio_ind;
	struct timeval time_out, *timer;
    int i;
    char ricarica_timer;
    stato='l';
    
    if (argc != 3)
	{
       printf("Numero argomenti errato.\nUsa %s <host remoto> <porta>\n", argv[0]);
       exit(1);
    }
    apri_connessione_server(argv[1], atoi(argv[2]));
    printf("Connessione al server %s (porta %s) effettuata con successo.\n\n", argv[1], argv[2]);
    
    /* visualizza i comandi di help */
    printf("%s\n", HELP);
    
    printf("Inserisci il tuo nome: ");
    scanf("%s", nome_utente);
    porta_udp=0;
    do
    {
      if (porta_udp > 65535)
         printf("Valore inserito non valido. Riprova.");
      printf("Inserisci la porta UDP di ascolto: ");
      scanf("%u", &porta_udp);
    } while (porta_udp > 65535);
    
    /* creazione socket udp */
    if ((socket_udp=socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
       perror("Creazione socket udp fallita!\n");
       exit(1);
    }
    bzero(&mio_ind, sizeof(mio_ind));
    mio_ind.sin_family=AF_INET;
    mio_ind.sin_port=htons(porta_udp);
    mio_ind.sin_addr.s_addr=htonl(INADDR_ANY);
    if (bind(socket_udp, (struct sockaddr*) &mio_ind, sizeof(mio_ind)) < 0)
    {
        perror("Errore nella bind: ");
        exit(1);
    }
    
    invia_dati_client();
    
    FD_ZERO(&set_lista);
    FD_SET(0, &set_lista); /* stdin */
    FD_SET(socket_server, &set_lista);
    FD_SET(socket_udp, &set_lista);
    
    ricevuta_hit=0;
	shell='>';
    while (1)
	{
          	if (ricarica_timer) /* si è ricevuta una comunicazione UDP o un comando da tastiera quindi si resetta il timer */
          	{
          		time_out.tv_sec=60;
    	  		time_out.tv_usec=0;
    	  	}
    	  	else /* il timer rimane come era quando è uscito dalla select, perchè si è ricevuto un messaggio dal server quindi il conteggio non va azzerato */
    	  		ricarica_timer=1;
			if (shell == '#') /* il timer si usa solo se è in corso una partita */
				timer=&time_out;
			else
				timer=NULL;
          
          if (!ricevuta_hit)
          {
          	printf("%c ", shell);
          	fflush(stdout);
          }
          else
          	ricevuta_hit=0;
          
          set_temp=set_lista;
          switch (select(socket_udp+1, &set_temp, NULL, NULL, timer))
          {
                 case -1: /* errore della select */
                 {
                      perror("Errore nella select(): ");
                      exit(1);
                 }
                 case 0: /* timer scaduto */
                 {
                 	  printf("Timer scaduto!\n");
                      disconnect();
                      exit(0);
                 }
                 default:
                 {
                         for (i=0; i<=socket_udp; i++)
                         {
                             if (FD_ISSET(i, &set_temp))
                             {
                                if (i == 0) {
                                	lettura_tastiera();
                                	continue ; }
                                if (i == socket_server) {
                                	lettura_server();
                                	ricarica_timer=0;
                                	continue ;}
                                if (i == socket_udp) {
                                	lettura_udp();
                                	continue ;}
                             	/* caso di default */
                             	printf("Situazione inprevista, caso default, switch, main.");
                             } /* fine if */
                         } /* fine for */
                 } /* fine default */
          } /* fine switch */
    } /* fine while */
} /* fine main */
