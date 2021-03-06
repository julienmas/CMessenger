// client.c
#include "pse.h"

#define CMD "client"

void sendUsername(int fd, char *username);
void sendPassword(int fd, char *password);
void sendUserAction(int fd, ACTION action, void *args);

void *receiveMessage(void *arg);
void mainMenu();
void str_trim_lf(char *arr, int length);
static void clearStdin();

void deserializeChatroom(int _fd, ChatRoom *destination);

int sock;
ChatRoom *currentChatroom;
ChatRooms *_chatroomsList;

int main(int argc, char *argv[])
{
  int ret;
  struct sockaddr_in *adrServ;
  int fin = FAUX;
  char ligne[LIGNE_MAX];
  char username[LIGNE_MAX];
  char password[LIGNE_MAX];
  pthread_t idThreadReceive; // thread qui gère la réception de messages des autres clients

  currentChatroom = (ChatRoom *)malloc(sizeof(ChatRoom));
  _chatroomsList = NULL;

  signal(SIGPIPE, SIG_IGN);

  if (argc != 5)
    erreur("usage: %s machine port username password\n", argv[0]);

  printf("%s: creating a socket\n", CMD);
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    erreur_IO("socket");

  printf("%s: DNS resolving for %s, port %s\n", CMD, argv[1], argv[2]);
  adrServ = resolv(argv[1], argv[2]);
  if (adrServ == NULL)
    erreur("adresse %s port %s inconnus\n", argv[1], argv[2]);

  printf("%s: adr %s, port %hu\n", CMD,
         stringIP(ntohl(adrServ->sin_addr.s_addr)),
         ntohs(adrServ->sin_port));

  printf("%s: connecting the socket\n", CMD);
  ret = connect(sock, (struct sockaddr *)adrServ, sizeof(struct sockaddr_in));
  if (ret < 0)
    erreur_IO("connect");

  // Récupération du pseudo
  strcpy(username, argv[3]);
  sendUsername(sock, username);
  if (strncmp(username, argv[3], LIGNE_MAX) != 0)
    printf("%s already used.\n", argv[3]);
  else
  {
    // Récupération du mot de passe
    strcpy(password, argv[4]);
    sendPassword(sock, password);
    if (strncmp(password, argv[4], LIGNE_MAX) != 0)
      printf("Password incorrect\n");
    else
    {
      mainMenu();

      // Sortie du menu principal

      system("clear");
      printf("--- ChatRoom n°%d : %s --- \n", currentChatroom->room_id, currentChatroom->name);

      // réception des messages
      pthread_create(&idThreadReceive, NULL, receiveMessage, NULL);
      //envoi des messages
      while (!fin)
      {
        printf("> ");
        if (fgets(ligne, LIGNE_MAX, stdin) == NULL)
          // sortie par CTRL-D
          fin = VRAI;
        else
        {

          if (ecrireLigne(sock, ligne) == -1)
            erreur_IO("ecriture socket");

          if (strncmp(ligne, "/list\n", LIGNE_MAX) == 0)
          {
            sendUserAction(sock, DISPLAY, NULL);
          }

          if (strncmp(ligne, "/fin\n", LIGNE_MAX) == 0)
            fin = VRAI;
        }
      }
    }
  }

  if (close(sock) == -1)
    erreur_IO("fermeture socket");

  exit(EXIT_SUCCESS);
}

/**
 *  Menu Principal 
 *  - Création d'une nouvelle chatroom
 *  - Rejoindre une chatroom existante
 * */
void mainMenu()
{
  char room_name[MAX_ROOM_NAME];

  int choice = 0;
  int room_choice = -1;

  while (VRAI)
  {
    // system("clear");

    printf("1. Créer une nouvelle ChatRoom\n");
    printf("2. Rejoindre une ChatRoom\n");

    // clearStdin();

    choice = getchar();

    switch (choice)
    {
    case '1':
      printf("Donnez un nom à la room :  ");
      clearStdin();
      fgets(room_name, MAX_ROOM_NAME, stdin);
      str_trim_lf(room_name, strlen(room_name));

      sendUserAction(sock, CREATE, room_name);

      break;

    case '2':
      sendUserAction(sock, DISPLAY, NULL);

      printf("Quelle room voulez vous rejoindre ? (id) ");
      scanf("%d", &room_choice);

      sendUserAction(sock, JOIN, &room_choice);

      if (currentChatroom->room_id != -1)
        return;

      printf("ID non valide, veuillez réessayer.\n");

      break;

    default:
      printf("Choix inconnu. Veuillez réessayer.\n");
      break;
    }
  }
}

/**
 * Fonction utilitaire pour retirer les espaces d'une chaine de caractères
 * */
void str_trim_lf(char *arr, int length)
{
  int i;
  for (i = 0; i < length; i++)
  { // trim \n
    if (arr[i] == '\n')
    {
      arr[i] = '\0';
      break;
    }
  }
}

/**
 * Fonction utilitaire pour vider le buffer stdin
 * */
static void clearStdin()
{
  int c;
  while ((c = getchar()) != EOF && c != '\n')
  {
  }
}

void sendUsername(int fd, char *username)
{
  if (write(fd, username, sizeof(username)) == -1)
    erreur_IO("ecriture socket");

  if (read(fd, username, sizeof(username)) == -1)
    erreur_IO("lecture socket");
}

void sendPassword(int fd, char *password)
{
  if (write(fd, password, sizeof(password)) == -1)
    erreur_IO("écriture socket");

  if (read(fd, password, sizeof(password)) == -1)
    erreur_IO("lecture socket");
}

/**
 * Envoi d'une action utilisateur au serveur
 * @params 
 *  - fd : socket de connection au serveur
 *  - action : action à éxecuter
 *  - args : arguments à envoyer avec l'Action
 * @return void
 * */
void sendUserAction(int fd, ACTION action, void *args)
{
  printf("%s: User Action %d \n", CMD, action);

  if (write(fd, &action, sizeof(action)) == -1)
    erreur_IO("ecriture socket");

  if (args != NULL)
  {
    printf("%s: Envoi de données\n", CMD);

    if (write(fd, args, MAX_ROOM_NAME) == -1)
      erreur_IO("ecriture socket");
  }

  if (action == DISPLAY)
  {
    printf("Voici la liste des Chat Rooms : \n");

    int fin = FAUX;

    while (!fin)
    {
      char room_name[LIGNE_MAX];

      if (read(fd, room_name, sizeof(room_name)) == -1)
        erreur_IO("lecture socket DISPLAY");

      if (strstr(room_name, "end_list") != NULL)
      {
        fin = VRAI;
        continue;
      }

      printf("%s", room_name);
    }
  }
  else
  {
    deserializeChatroom(fd, currentChatroom);
  }
}

/**
 * Fonction du thread affichant les messages des autres clients sur le client actuel
 * */
void *receiveMessage(void *arg)
{
  char ligne[LIGNE_MAX];

  while (1)
  {
    if (lireLigne(sock, ligne) == -1)
      erreur_IO("lecture socket");
    printf("%s\n", ligne);
  }
}

/**
 * Fonction utilisée pour la déserialisation d'un object de type <struct ChatRoom>
 * @params :
 *  - _fd : socket de connection
 *  - destination : <struct ChatRoom> à récupérer depuis le canal _fd
 * */
void deserializeChatroom(int _fd, ChatRoom *destination)
{

  char _name[MAX_ROOM_NAME];
  int _room_id;
  int _nbr_clients;

  if (read(_fd, _name, sizeof(_name)) == -1)
    erreur_IO("lecture socket ACTION");

  if (read(_fd, &_room_id, sizeof(_room_id)) == -1)
    erreur_IO("lecture socket ACTION");

  if (read(_fd, &_nbr_clients, sizeof(_nbr_clients)) == -1)
    erreur_IO("lecture socket ACTION");

  strcpy(destination->name, _name);
  destination->room_id = _room_id;
  destination->nbr_clients = _nbr_clients;
}