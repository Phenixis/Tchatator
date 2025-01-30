#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

void nettoyer_buffer() {
    while (getchar() != '\n'); // Consomme les caractères jusqu'au '\n'
}

void afficher_menu(char *role) {
    printf("\n=== Menu ===\n");
    printf("1.  Me connecter\n");
    printf("2.  Envoyer un message\n");
    printf("3.  Me déconnecter et quitter\n");

    // Si connecté
    if (strcmp(role, "membre") == 0 || strcmp(role, "pro") == 0) {
        printf("4.  Messages non lus\n");
        printf("5.  Informations concernant un de mes messages\n");
        printf("6.  Modifier un de mes messages\n");
    }

    // Si pro
    if (strcmp(role, "pro") == 0) {
        printf("7.  Historique des messages avec un client\n");
        printf("8.  Bloquer un client 24h\n");
        printf("9.  Bannir un client définitivement\n");
        printf("10. Lever le ban d'un client\n");
    }

    printf("Choisissez une option: ");
}

// Mettre à jour le rôle du client (pro / membre / admin)
void update_role(int sock, char *role) {
    send(sock, "/role", 6, 0);

    // Recevoir jusqu'à 19 caractères (en réservant 1 octet pour '\0')
    int bytes_received = recv(sock, role, 19, 0);
    if (bytes_received >= 0) {
        role[bytes_received] = '\0';  // Assurez-vous que la chaîne est correctement terminée
    } else {
        // Gérer les erreurs de réception ici (par exemple, socket déconnectée)
        perror("Erreur lors de la réception du rôle");
    }
}

void connexion(int sock) {
    char *api_key = malloc(100 * sizeof(char));  // Allocation de mémoire pour api_key

    printf("Entrez votre clé API: ");
    scanf("%s", api_key);
    getchar();  // Pour consommer le newline laissé par scanf

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

    free(api_key);  // N'oubliez pas de libérer la mémoire allouée pour api_key
}

void deconnexion(int sock) {
    char *requete = "/deconnexion";
    send(sock, requete, strlen(requete), 0);

    close(sock);
    exit(0);
}

void envoyer_message(int sock) {
    int id_compte;
    char message[1024];

    printf("Entrez l'ID du compte: ");
    scanf("%d", &id_compte);
    getchar();  // Pour consommer le newline laissé par scanf

    printf("Entrez le message à envoyer: ");
    fgets(message, sizeof(message), stdin);
    message[strcspn(message, "\n")] = 0;  // Supprimer le newline à la fin du message

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

void messages_non_lus(int sock) {
    char *requete = "/liste";
    send(sock, requete, strlen(requete), 0);

    close(sock);
    exit(0);
}

// Traitement de la commande
void traiter_commande(int choix, int sock, char *role) {
    switch (choix) {
        case 1:
            connexion(sock);
            update_role(sock, role);  // Met à jour le rôle après la connexion
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
            if (strcmp(role, "membre") || strcmp(role, "pro")) {
                messages_non_lus(sock);
            } else {
                printf("Option non disponible pour votre rôle.\n");
            }
            break;

        case 5:
            if (strcmp(role, "membre") || strcmp(role, "pro")) {
                // Traite l'option "Informations concernant un message"
                printf("Informations sur un message...\n");
            } else {
                printf("Option non disponible pour votre rôle.\n");
            }
            break;

        case 6:
            if (strcmp(role, "membre") || strcmp(role, "pro")) {
                // Traite l'option "Modifier un message"
                printf("Modification d'un message...\n");
            } else {
                printf("Option non disponible pour votre rôle.\n");
            }
            break;

        case 7:
            if (strcmp(role, "pro")) {
                // Traite l'option "Historique des messages"
                printf("Historique des messages avec un client...\n");
            } else {
                printf("Option non disponible pour votre rôle.\n");
            }
            break;

        case 8:
            if (strcmp(role, "pro")) {
                // Traite l'option "Bloquer un client 24h"
                printf("Bloquage d'un client...\n");
            } else {
                printf("Option non disponible pour votre rôle.\n");
            }
            break;

        case 9:
            if (strcmp(role, "pro")) {
                // Traite l'option "Bannir un client définitivement"
                printf("Bannissement d'un client...\n");
            } else {
                printf("Option non disponible pour votre rôle.\n");
            }
            break;

        case 10:
            if (strcmp(role, "pro")) {
                // Traite l'option "Lever le ban d'un client"
                printf("Levée du ban d'un client...\n");
            } else {
                printf("Option non disponible pour votre rôle.\n");
            }
            break;

        default:
            printf("Option invalide, essayez encore.\n");
    }
}

int main(int argc, char *argv[]) {
    // Vérifier si un port est passé en paramètre
    if (argc <= 1) {
        perror("Aucun port spécifié");
        exit(1);
    }

    int port = atoi(argv[1]);
    if (!port) {
        perror("Veuilleez saisir un port valide en paramètre");
        exit(1);
    }

    int sock;
    struct sockaddr_in server_addr;
    // Créer le socket
    sock = socket(AF_INET, SOCK_STREAM, 0); // Connexion en protocole TCP
    if (sock == -1) {
        perror("Création de la socket échouée");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Connexion au serveur local

    // Tentative de connexion avec le serveur
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Impossible de se connecter au Tchatator");
        exit(1);
    }

    int choix;
    char role[20] = "aucun";

    // Boucle principale pour afficher le menu et traiter les options
    while (1) {
        afficher_menu(role);
        
        // Lire l'option avec fgets pour éviter les caractères invalides
        char buffer[10];
        fgets(buffer, sizeof(buffer), stdin);

        // Vérifier si l'entrée est un nombre valide
        if (sscanf(buffer, "%d", &choix) != 1) {
            printf("Entrée invalide, veuillez saisir un nombre.\n");
            nettoyer_buffer();  // Nettoie le tampon pour éviter les caractères restants
            continue;  // Redemander une entrée
        }

        traiter_commande(choix, sock, role);

    }

    return 0;
}
