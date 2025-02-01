#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

void nettoyer_buffer()
{
    while (getchar() != '\n')
        ; // Consomme les caractères jusqu'au '\n'
}

void afficher_menu(int sock, char *role)
{
    printf("\n=== Menu ===\n");
    printf("1.  Me connecter\n");
    printf("2.  Envoyer un message\n");
    printf("3.  Me déconnecter et quitter\n");

    // Si connecté
    if (strcmp(role, "membre") == 0 || strcmp(role, "pro") == 0)
    {
        // Connaître le nombre de messages non lus
        char *requete = "/nb_non_lus";
        int nb_non_lus = 0;
        send(sock, requete, strlen(requete), 0);
        recv(sock, &nb_non_lus, sizeof(int), 0);

        printf("4.  Messages non lus (%d)\n", nb_non_lus);
        printf("5.  Informations concernant un de mes messages\n");
        printf("6.  Modifier un de mes messages\n");
        printf("7.  Supprimer un de mes messages\n");
    }

    // Si pro
    if (strcmp(role, "pro") == 0)
    {
        printf("8.  Historique des messages avec un client\n");
        printf("9.  Bloquer un client 24h\n");
        printf("10  Lever le blocage d'un client 24h\n");
        printf("11. Bannir un client définitivement\n");
        printf("12. Lever le ban d'un client\n");
    }

    printf("Choisissez une option: ");
}

// Mettre à jour le rôle du client (pro / membre / admin)
void update_role(int sock, char *role)
{
    send(sock, "/role", 6, 0);

    // Recevoir jusqu'à 19 caractères (en réservant 1 octet pour '\0')
    int bytes_received = recv(sock, role, 19, 0);
    if (bytes_received >= 0)
    {
        role[bytes_received] = '\0'; // Assurez-vous que la chaîne est correctement terminée
    }
    else
    {
        // Gérer les erreurs de réception ici (par exemple, socket déconnectée)
        perror("Erreur lors de la réception du rôle");
    }
}

void connexion(int sock)
{
    char *api_key = malloc(100 * sizeof(char)); // Allocation de mémoire pour api_key

    printf("Entrez votre clé API: ");
    scanf("%s", api_key);
    getchar(); // Pour consommer le newline laissé par scanf

    // Construire la requête /connexion
    char requete[1024];
    snprintf(requete, sizeof(requete), "/connexion %s", api_key);

    // Envoyer la requête au serveur
    send(sock, requete, strlen(requete), 0);
    printf("Message envoyé: %s\n", requete);

    // Recevoir la réponse du serveur
    char buffer[1024];
    recv(sock, buffer, sizeof(buffer), 0);
    printf("Réponse du serveur: %s", buffer);

    free(api_key); // N'oubliez pas de libérer la mémoire allouée pour api_key
}

void deconnexion(int sock)
{
    char *requete = "/deconnexion";
    send(sock, requete, strlen(requete), 0);

    close(sock);
    exit(0);
}

void envoyer_message(int sock)
{
    int id_compte;
    char message[1024];

    printf("Entrez l'ID du compte: ");
    scanf("%d", &id_compte);
    getchar(); // Pour consommer le newline laissé par scanf

    printf("Entrez le message à envoyer: ");
    fgets(message, sizeof(message), stdin);
    message[strcspn(message, "\n")] = 0; // Supprimer le newline à la fin du message

    // Construire la requête /message
    char requete[1224];
    snprintf(requete, sizeof(requete), "/message %d %s", id_compte, message);

    // Envoyer la requête au serveur
    send(sock, requete, strlen(requete), 0);
    printf("Message envoyé: %s\n", requete);

    // Recevoir la réponse du serveur
    char buffer[1024];
    recv(sock, buffer, sizeof(buffer), 0);
    printf("Réponse du serveur: %s", buffer);
}

// ######################
// FONCTIONS A TERMINER #
// ######################
char *trim_newline(const char *str)
{
    int len = strlen(str);
    char *trimmed_str = (char *)malloc(len + 1);
    if (!trimmed_str)
    {
        perror("malloc failed");
        exit(1);
    }
    strcpy(trimmed_str, str);
    while (len > 0 && (trimmed_str[len - 1] == '\n' || trimmed_str[len - 1] == '\r' || isspace(trimmed_str[len - 1])))
    {
        trimmed_str[len - 1] = '\0';
        len--;
    }
    return trimmed_str;
}

void messages_non_lus(int sock)
{
    // Demander au serveur les messages non lus
    char *requete = "/liste";
    send(sock, requete, strlen(requete), 0);

    // rendre_socket_non_bloquante(sock);

    char buffer[1024];
    ssize_t bytes_received;

    bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

    // Reçu qqch (message ou 204/NO CONTENT)
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';

        printf("\n---------------------\n");
        // CAS NO CONTENT
        if (strcmp(trim_newline(buffer), "204/NO CONTENT") == 0)
        {
            printf("Aucun nouveau message\n");
        }
        // CAS PAS LE BON ROLE
        else if (strcmp(trim_newline(buffer), "416/RANGE NOT SATISFIABLE") == 0)
        {
            printf("Votre rôle actuel ne permet pas d'avoir des messages\n");
        }
        // CAS INTERNAL SERVER ERROR
        else if (strcmp(trim_newline(buffer), "500/INTERNAL SERVER ERROR") == 0)
        {
            printf("Le serveur a rencontré un problème lors de la récuparation des messages\n");
        }
        // CAS MESSAGE EFFECTIF
        else
        {
            if (bytes_received >= 7 && strncmp(buffer, "200/OK\n", 7) == 0)
            {
                memmove(buffer, buffer + 7, bytes_received - 7);
                bytes_received -= 7; // Réduire la taille du message
            }
            printf("%.*s", (int)(bytes_received - 7), buffer);
        }
        printf("---------------------\n");
    }
    else if (bytes_received == 0)
    {
        printf("Le serveur a terminé l'envoi des messages.\n");
    }
    else if (bytes_received == -1)
    {
        perror("Erreur de réception des données\n");
    }
}

void info_message(int sock)
{
    char *requete = "/liste";
    send(sock, requete, strlen(requete), 0);
}

void modifier_message(int sock)
{
    char *requete = "/liste";
    send(sock, requete, strlen(requete), 0);
}

void supprimer_message(int sock)
{
    char *requete = "/liste";
    send(sock, requete, strlen(requete), 0);
}

void historique_message(int sock)
{
    char *requete = "/liste";
    send(sock, requete, strlen(requete), 0);
}

void bloquer_client(int sock)
{
    char *requete = "/liste";
    send(sock, requete, strlen(requete), 0);

    close(sock);
    exit(0);
}

void enlever_blocage(int sock)
{
    char *requete = "/liste";
    send(sock, requete, strlen(requete), 0);
}

void bannir_client(int sock)
{
    char *requete = "/liste";
    send(sock, requete, strlen(requete), 0);
}

void enlever_ban(int sock)
{
    char *requete = "/liste";
    send(sock, requete, strlen(requete), 0);
}
// #######################
// /FONCTIONS A TERMINER #
// #######################

// Traitement de la commande de l'utilisateur (avec vérification de rôle)
void traiter_commande(int choix, int sock, char *role)
{
    switch (choix)
    {
    case 1:
        connexion(sock);
        update_role(sock, role); // Met à jour le rôle après la connexion
        break;
    case 2:
        envoyer_message(sock);
        break;
    case 3:
        printf("Merci d'avoir utilisé Tchatator...\n");
        deconnexion(sock);
        break;

    // Options disponibles selon le rôle
    case 4:
        if (strcmp(role, "membre") || strcmp(role, "pro"))
        {
            messages_non_lus(sock);
        }
        else
        {
            printf("Option non disponible pour votre rôle.\n");
        }
        break;

    case 5:
        if (strcmp(role, "membre") || strcmp(role, "pro"))
        {
            info_message(sock);
        }
        else
        {
            printf("Option non disponible pour votre rôle.\n");
        }
        break;

    case 6:
        if (strcmp(role, "membre") || strcmp(role, "pro"))
        {
            modifier_message(sock);
        }
        else
        {
            printf("Option non disponible pour votre rôle.\n");
        }
        break;

    case 7:
        if (strcmp(role, "membre") || strcmp(role, "pro"))
        {
            supprimer_message(sock);
        }
        else
        {
            printf("Option non disponible pour votre rôle.\n");
        }
        break;

    case 8:
        if (strcmp(role, "pro"))
        {
            historique_message(sock);
        }
        else
        {
            printf("Option non disponible pour votre rôle.\n");
        }
        break;

    case 9:
        if (strcmp(role, "pro"))
        {
            bloquer_client(sock);
        }
        else
        {
            printf("Option non disponible pour votre rôle.\n");
        }
        break;

    case 10:
        if (strcmp(role, "pro"))
        {
            enlever_blocage(sock);
        }
        else
        {
            printf("Option non disponible pour votre rôle.\n");
        }
        break;

    case 11:
        if (strcmp(role, "pro"))
        {
            bannir_client(sock);
        }
        else
        {
            printf("Option non disponible pour votre rôle.\n");
        }
        break;

    case 12:
        if (strcmp(role, "pro"))
        {
            enlever_ban(sock);
        }
        else
        {
            printf("Option non disponible pour votre rôle.\n");
        }
        break;

    default:
        printf("Option invalide, essayez encore.\n");
    }
}

int main(int argc, char *argv[])
{
    // Vérifier si un port est passé en paramètre
    if (argc <= 1)
    {
        perror("Aucun port spécifié");
        exit(1);
    }

    int port = atoi(argv[1]);
    if (!port)
    {
        perror("Veuilleez saisir un port valide en paramètre");
        exit(1);
    }

    int sock;
    struct sockaddr_in server_addr;
    // Créer le socket
    sock = socket(AF_INET, SOCK_STREAM, 0); // Connexion en protocole TCP
    if (sock == -1)
    {
        perror("Création de la socket échouée");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Connexion au serveur local

    // Tentative de connexion avec le serveur
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Impossible de se connecter au Tchatator");
        exit(1);
    }

    int choix;
    char role[20] = "aucun";

    // Boucle principale pour afficher le menu et traiter les options
    while (1)
    {
        afficher_menu(sock, role);

        // Lire l'option avec fgets pour éviter les caractères invalides
        char buffer[10];
        fgets(buffer, sizeof(buffer), stdin);

        // Vérifier si l'entrée est un nombre valide
        if (sscanf(buffer, "%d", &choix) != 1)
        {
            printf("Entrée invalide, veuillez saisir un nombre.\n");
            nettoyer_buffer(); // Nettoie le tampon pour éviter les caractères restants
            continue;          // Redemander une entrée
        }

        traiter_commande(choix, sock, role);
    }

    return 0;
}
