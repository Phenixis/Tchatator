#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

void nettoyer_buffer() {
    while (getchar() != '\n'); // Consomme les caractères jusqu'au '\n'
}

void afficher_menu() {
    printf("\n=== Menu ===\n");
    printf("1. Se connecter\n");
    printf("2. Envoyer un message\n");
    printf("3. Se déconnecter et quitter\n");
    printf("Choisissez une option: ");
}

void connexion(int sock) {
    char *api_key;

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

    // Boucle principale pour afficher le menu et traiter les options
    while (1) {
        afficher_menu();
        
        // Lire l'option avec fgets pour éviter les caractères invalides
        char buffer[10];
        fgets(buffer, sizeof(buffer), stdin);

        // Vérifier si l'entrée est un nombre valide
        if (sscanf(buffer, "%d", &choix) != 1) {
            printf("Entrée invalide, veuillez saisir un nombre.\n");
            nettoyer_buffer();  // Nettoie le tampon pour éviter les caractères restants
            continue;  // Redemander une entrée
        }

        switch (choix) {
            case 1:
                connexion(sock);
                break;
            case 2:
                envoyer_message(sock);
                break;
            case 3:
                printf("Merci d'avoir utilisé Tchatator...\n");
                deconnexion(sock);
                break;
            default:
                printf("Option invalide, essayez encore.\n");
        }
    }

    return 0;
}
