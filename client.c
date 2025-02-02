#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

void nettoyer_buffer(void)
{
    char buffer[100];                     // Un buffer assez grand pour lire toute la ligne
    fgets(buffer, sizeof(buffer), stdin); // Lire et ignorer toute la ligne restante
}

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

/*
Retourne 1 si un code spécifique a été détecté et un message ad hoc affiché
*/
int code_to_message(char* code_message) {
    int returned = 1;
    if (strcmp(trim_newline(code_message), "429/TOO MANY REQUESTS MINUTE") == 0)
    {
        printf("Vous avez envoyé trop de requêtes dans la même minute. Patientez.\n");
    }
    else if (strcmp(trim_newline(code_message), "430/TOO MANY REQUESTS HOUR") == 0)
    {
        printf("Vous avez envoyé trop de requêtes dans la même heure. Revenez ultérieurement.\n");
    } else {
        returned = 0;
    }
    return returned;
}

void afficher_menu(int sock, char *role)
{
    // Si membre
    if (strcmp(role, "membre") == 0)
    {
        printf("\n=== Menu ===\n");
        printf("1. Envoyer un message\n");
        char *requete = "/nb_non_lus";
        int nb_non_lus = 0;
        send(sock, requete, strlen(requete), 0);
        recv(sock, &nb_non_lus, sizeof(int), 0);
        printf("2. Messages non lus (%d)\n", nb_non_lus);
        printf("3. Informations concernant un de mes messages\n");
        printf("4. Modifier un de mes messages\n");
        printf("5. Supprimer un de mes messages\n");
        printf("6. Historique des messages avec un client\n");
        printf("7. Me déconnecter et quitter\n");
    }
    else if (strcmp(role, "pro") == 0)
    {
        printf("\n=== Menu ===\n");
        printf("1. Envoyer un message\n");
        char *requete = "/nb_non_lus";
        int nb_non_lus = 0;
        send(sock, requete, strlen(requete), 0);
        recv(sock, &nb_non_lus, sizeof(int), 0);
        printf("2. Messages non lus (%d)\n", nb_non_lus);
        printf("3. Informations concernant un de mes messages\n");
        printf("4. Modifier un de mes messages\n");
        printf("5. Supprimer un de mes messages\n");
        printf("6. Historique des messages avec un client\n");
        printf("7. Bloquer un client 24h\n");
        printf("8. Lever le blocage d'un client\n");
        printf("9. Me déconnecter et quitter\n");
    }
    else if (strcmp(role, "admin") == 0)
    {
        printf("\n=== Menu ===\n");
        printf("1. Bloquer un client 24h\n");
        printf("2. Lever le blocage d'un client\n");
        printf("3. Bannir un client définitivement\n");
        printf("4. Lever le ban d'un client\n");
        printf("5. Synchroniser les paramètres\n");
        printf("6. Afficher les logs\n");
        printf("7. Me déconnecter et quitter\n");
        printf("8. Fermer le service Tchatator\n");
    }
    else
    {
        printf("\n=== Menu ===\n");
        printf("1. Me connecter\n");
        printf("2. Quitter\n");
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

    // Recevoir la réponse du serveur
    char buffer[1024];
    recv(sock, buffer, sizeof(buffer), 0);
    if (!code_to_message(buffer) && (buffer, "200/OK") == 0) {
        printf("Connecté\n");
    }

    free(api_key); // Ne pas oublier de libérer la mémoire allouée pour api_key
}

void deconnexion(int sock)
{
    printf("Merci d'avoir utilisé Tchatator...\n");
    char *requete = "/deconnexion";
    send(sock, requete, strlen(requete), 0);

    close(sock);
    exit(0);
}

void synchroniser_params(int sock)
{
    char *requete = "/sync";
    send(sock, requete, strlen(requete), 0);

    char buffer[1024];
    ssize_t len = recv(sock, buffer, sizeof(buffer), 0);
    buffer[len] = '\0';
    printf("Réponse du serveur: %s", buffer);
}

void logs(int sock)
{
    int nb_logs;
    char input[10];
    printf("Nombre de logs (facultatif, par défaut à 50) : ");    
    if (fgets(input, sizeof(input), stdin) != NULL)
    {
        // Si l'utilisateur appuie juste sur entrée (input est une chaîne vide)
        if (input[0] == '\n')
        {
            nb_logs = 50;
        }
        else
        {
            nb_logs = atoi(input);
        }
    }

    char requete[1224];
    snprintf(requete, sizeof(requete), "/logs %d", nb_logs);
    send(sock, requete, strlen(requete), 0);

    char buffer[1024]; // Tampon pour réception
    ssize_t bytes_received;
    char full_message[10000];  // Tampon pour accumuler le message complet
    size_t total_received = 0; // Taille totale des données reçues

    while (1)
    {
        // Préparer l'ensemble de descripteurs pour select()
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        // Définir le timeout à 200ms
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000; // 200ms

        // Appel select pour attendre des données ou un timeout
        int ret = select(sock + 1, &readfds, NULL, NULL, &timeout);

        if (ret == -1)
        {
            // En cas d'erreur
            perror("Erreur de select");
            break;
        }
        else if (ret == 0)
        {
            // Plus rien reçu dans les dernières 200ms, sortie de la boucle
            if (total_received > 0)
            {
                if (!code_to_message(full_message) && strcmp(trim_newline(full_message), "204/NO CONTENT") == 0)
                {
                    printf("Il n'y a pas de logs\n");
                }
                else if (strcmp(trim_newline(full_message), "403/FORBIDDEN") == 0)
                {
                    printf("Votre rôle actuel ne vous permet pas d'avoir les logs\n");
                }
                else if (strcmp(trim_newline(full_message), "500/INTERNAL SERVER ERROR") == 0)
                {
                    printf("Erreur du serveur lors de la lecture des logs");
                }
            }
            break;
        }
        else
        {
            // Il y a des données à recevoir
            bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0';

                // Copier les données reçues dans le tampon complet
                if (total_received + bytes_received < sizeof(full_message))
                {
                    memcpy(full_message + total_received, buffer, bytes_received);
                    total_received += bytes_received;
                }
                else
                {
                    fprintf(stderr, "Le tampon de réception est plein, données perdues.\n");
                    break;
                }

                // Vérifier si un message complet a été reçu (en supposant que chaque message se termine par "\n\n")
                if (strstr(full_message, "\n") != NULL)
                {
                    // Traiter le message complet
                    if (strncmp(full_message, "200/OK\n", 7) == 0)
                    {
                        memmove(full_message, full_message + 7, strlen(full_message) - 6);
                    }

                    if (!code_to_message(full_message)) {
                        printf("%.*s", (int)(total_received), full_message);
                    }

                    // Réinitialiser les tampons pour recevoir un autre message
                    total_received = 0;
                    memset(full_message, 0, sizeof(full_message));
                }
            }
            else if (bytes_received == 0)
            {
                // Le serveur a fermé la connexion
                printf("Le serveur a clos la connexion.\n");
                break;
            }
            else if (bytes_received == -1)
            {
                // Autres erreurs de réception
                perror("Erreur de réception des données\n");
                break; // Sortir de la boucle en cas d'erreur
            }
        }
    }
}

void fermer_service(int sock)
{
    char *requete = "/quit";
    send(sock, requete, strlen(requete), 0);

    char buffer[1024];
    ssize_t len = recv(sock, buffer, sizeof(buffer), 0);
    buffer[len] = '\0';
    printf("Réponse du serveur: %s", buffer);

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

    // Recevoir la réponse du serveur
    char buffer[1024];
    recv(sock, buffer, sizeof(buffer), 0);
    if (!code_to_message(buffer)) {
        printf("%s", buffer);
    }
}

void messages_non_lus(int sock)
{
    int bloc;
    char input[10];

    printf("N°page (facultatif, par défaut à 0) : ");
    if (fgets(input, sizeof(input), stdin) != NULL)
    {
        // Si l'utilisateur appuie juste sur entrée (input est une chaîne vide)
        if (input[0] == '\n')
        {
            bloc = 0;
        }
        else
        {
            bloc = atoi(input);
        }
    }

    // Demander au serveur les messages non lus
    char requete[1024];
    snprintf(requete, sizeof(requete), "/liste ?page=%d", bloc);
    send(sock, requete, strlen(requete), 0);

    char buffer[1024]; // Tampon pour réception
    ssize_t bytes_received;
    char full_message[10000];  // Tampon pour accumuler le message complet
    size_t total_received = 0; // Taille totale des données reçues

    // Boucle pour recevoir les messages
    printf("\n------------------------------------------\n");
    while (1)
    {
        // Préparer l'ensemble de descripteurs pour select()
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        // Définir le timeout à 200ms
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000; // 200ms

        // Appel select pour attendre des données ou un timeout
        int ret = select(sock + 1, &readfds, NULL, NULL, &timeout);

        if (ret == -1)
        {
            // En cas d'erreur
            perror("Erreur de select");
            break;
        }
        else if (ret == 0)
        {
            // Plus rien reçu dans les dernières 200ms, sortie de la boucle
            if (total_received > 0)
            {
                if (!code_to_message(full_message) && strcmp(trim_newline(full_message), "204/NO CONTENT") == 0)
                {
                    printf("Vous n'avez aucun nouveau message\n");
                }
                else if (strcmp(trim_newline(full_message), "403/FORBIDDEN") == 0)
                {
                    printf("Votre rôle actuel ne vous permet pas d'avoir des messages\n");
                }
                else if (strcmp(trim_newline(full_message), "416/RANGE NOT SATISFIABLE") == 0)
                {
                    printf("La page que vous demandez n'existe pas\n");
                }
                else if (strcmp(trim_newline(full_message), "429/TOO MANY REQUESTS MINUTE") == 0)
                {
                    printf("Vous avez envoyé trop de requêtes dans la même minute. Patientez.\n");
                }
                else if (strcmp(trim_newline(full_message), "430/TOO MANY REQUESTS HOUR") == 0)
                {
                    printf("Vous avez envoyé trop de requêtes dans la même heure. Revenez ultérieurement.\n");
                }
                else if (strcmp(trim_newline(full_message), "500/INTERNAL SERVER ERROR") == 0)
                {
                    printf("Erreur du serveur lors de la lecture de vos messages");
                }
            }
            break;
        }
        else
        {
            // Il y a des données à recevoir
            bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0';

                // Copier les données reçues dans le tampon complet
                if (total_received + bytes_received < sizeof(full_message))
                {
                    memcpy(full_message + total_received, buffer, bytes_received);
                    total_received += bytes_received;
                }
                else
                {
                    fprintf(stderr, "Le tampon de réception est plein, données perdues.\n");
                    break;
                }

                // Vérifier si un message complet a été reçu (en supposant que chaque message se termine par "\n\n")
                if (strstr(full_message, "\n\n") != NULL)
                {
                    // Traiter le message complet
                    if (strncmp(full_message, "200/OK\n", 7) == 0)
                    {
                        memmove(full_message, full_message + 7, strlen(full_message) - 6);
                    }

                    if (!code_to_message(full_message)) {
                        printf("%.*s", (int)(total_received), full_message);
                    }

                    // Réinitialiser les tampons pour recevoir un autre message
                    total_received = 0;
                    memset(full_message, 0, sizeof(full_message));
                }
            }
            else if (bytes_received == 0)
            {
                // Le serveur a fermé la connexion
                printf("Le serveur a clos la connexion.\n");
                break;
            }
            else if (bytes_received == -1)
            {
                // Autres erreurs de réception
                perror("Erreur de réception des données\n");
                break; // Sortir de la boucle en cas d'erreur
            }
        }
    }
    printf("------------------------------------------\n");
}

void supprimer_message(int sock)
{
    char *id_message = malloc(15 * sizeof(char));

    printf("Entrez l'id du message : ");
    scanf("%s", id_message);
    getchar(); // Pour consommer le newline laissé par scanf

    // Construire la requête /supprime
    char requete[1024];
    snprintf(requete, sizeof(requete), "/supprime %s", id_message);

    // Envoyer la requête au serveur
    send(sock, requete, strlen(requete), 0);

    // Recevoir la réponse du serveur
    char buffer[1024];
    int recv_bytes = recv(sock, buffer, sizeof(buffer), 0);
    buffer[recv_bytes] = '\0';
    if (!code_to_message(buffer)) {
        printf("%s", buffer);
    }
    free(id_message);
}

void x_messages_precedents(int sock, char *id_client)
{
    int peut_naviguer;
    char buffer[1024];
    char *id_message = malloc(15 * sizeof(char));
    printf("(Tapez 'q' pour quitter la navigation)\n");
    printf("Entrez l'id du message dont vous souhaitez voir les messages précédents : ");
    scanf("%s", id_message);
    getchar(); // Pour consommer le newline laissé par scanf

    if (strcmp(id_message, "q") == 0)
    {
        return;
    }

    // Envoyer la requête au serveur
    char requete[1024];
    snprintf(requete, sizeof(requete), "/precedent %s %s", id_client, id_message);
    send(sock, requete, strlen(requete), 0);

    ssize_t bytes_received;
    char full_message[10000];  // Tampon pour accumuler le message complet
    size_t total_received = 0; // Taille totale des données reçues

    // Boucle pour recevoir les messages
    printf("\n------------------------------------------\n");
    while (1)
    {
        // Préparer l'ensemble de descripteurs pour select()
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        // Définir le timeout à 200ms
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000; // 200ms

        // Appel select pour attendre des données ou un timeout
        int ret = select(sock + 1, &readfds, NULL, NULL, &timeout);

        if (ret == -1)
        {
            // En cas d'erreur
            perror("Erreur de select");
            break;
        }
        else if (ret == 0)
        {
            // Plus rien reçu dans les dernières 200ms, sortie de la boucle
            if (total_received > 0)
            {
                if (!code_to_message(full_message) && strcmp(trim_newline(full_message), "404/NOT FOUND") == 0)
                {
                    printf("Ce le client ou message spécifié n'existe pas\n");
                }
                else if (strcmp(trim_newline(full_message), "204/NO CONTENT") == 0)
                {
                    printf("Vous n'avez plus de message précédent\n");
                }
                else if (strcmp(trim_newline(full_message), "403/FORBIDDEN") == 0)
                {
                    printf("Votre rôle actuel ne vous permet pas d'avoir des messages\n");
                }
                else if (strcmp(trim_newline(full_message), "500/INTERNAL SERVER ERROR") == 0)
                {
                    printf("Erreur du serveur lors de la lecture de vos messages");
                }
            }
            break;
        }
        else
        {
            // Il y a des messages à afficher
            bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0';

                // Copier les données reçues dans le tampon complet
                if (total_received + bytes_received < sizeof(full_message))
                {
                    memcpy(full_message + total_received, buffer, bytes_received);
                    total_received += bytes_received;
                }
                else
                {
                    fprintf(stderr, "Le tampon de réception est plein, données perdues.\n");
                    break;
                }

                // Vérifier si un message complet a été reçu (en supposant que chaque message se termine par "\n\n")
                if (strstr(full_message, "\n\n") != NULL)
                {
                    // Traiter le message complet
                    if (strncmp(full_message, "200/OK\n", 7) == 0)
                    {
                        memmove(full_message, full_message + 7, strlen(full_message) - 6);
                    }

                    if (!code_to_message(full_message)) {
                        peut_naviguer = 1;
                        printf("%.*s", (int)(total_received), full_message);
                    }

                    // Réinitialiser les tampons pour recevoir un autre message
                    total_received = 0;
                    memset(full_message, 0, sizeof(full_message));
                }
            }
            else if (bytes_received == 0)
            {
                // Le serveur a fermé la connexion
                printf("Le serveur est clos.\n");
                break; // Sortir de la boucle
            }
            else if (bytes_received == -1)
            {
                // Autres erreurs de réception
                perror("Erreur de réception des données\n");
                break; // Sortir de la boucle en cas d'erreur
            }
        }
    }
    printf("------------------------------------------\n");

    if (peut_naviguer)
    {
        x_messages_precedents(sock, id_client);
    }
}

void historique_message(int sock)
{
    char *id_client = malloc(15 * sizeof(char));
    int bloc;
    char input[10];
    char buffer[1024];
    int peut_naviguer = 0;

    printf("Entrez l'id d'un autre client : ");
    scanf("%s", id_client);
    getchar(); // Pour consommer le newline laissé par scanf

    printf("N°page (facultatif, par défaut à 0) : ");
    if (fgets(input, sizeof(input), stdin) != NULL)
    {
        // Si l'utilisateur appuie juste sur entrée (input est une chaîne vide)
        if (input[0] == '\n')
        {
            bloc = 0;
        }
        else
        {
            bloc = atoi(input);
        }
    }

    // Envoyer la requête au serveur
    char requete[1024];
    snprintf(requete, sizeof(requete), "/conversation %s ?page=%d", id_client, bloc);
    send(sock, requete, strlen(requete), 0);

    ssize_t bytes_received;
    char full_message[10000];  // Tampon pour accumuler le message complet
    size_t total_received = 0; // Taille totale des données reçues

    // Boucle pour recevoir les messages
    printf("\n------------------------------------------\n");
    while (1)
    {
        // Préparer l'ensemble de descripteurs pour select()
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        // Définir le timeout à 200ms
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000; // 200ms

        // Appel select pour attendre des données ou un timeout
        int ret = select(sock + 1, &readfds, NULL, NULL, &timeout);

        if (ret == -1)
        {
            // En cas d'erreur
            perror("Erreur de select");
            break;
        }
        else if (ret == 0)
        {
            // Plus rien reçu dans les dernières 200ms, sortie de la boucle
            if (total_received > 0)
            {
                if (!code_to_message(full_message) && strcmp(trim_newline(full_message), "404/NOT FOUND") == 0)
                {
                    printf("Ce client n'existe pas\n");
                }
                else if (strcmp(trim_newline(full_message), "204/NO CONTENT") == 0)
                {
                    printf("Vous n'avez jamais échangé avec ce client\n");
                }
                else if (strcmp(trim_newline(full_message), "403/FORBIDDEN") == 0)
                {
                    printf("Votre rôle actuel ne vous permet pas d'avoir des messages\n");
                }
                else if (strcmp(trim_newline(full_message), "416/RANGE NOT SATISFIABLE") == 0)
                {
                    printf("La page demandée n'existe pas\n");
                }
                else if (strcmp(trim_newline(full_message), "500/INTERNAL SERVER ERROR") == 0)
                {
                    printf("Erreur du serveur lors de la lecture de vos messages");
                }
            }
            break;
        }
        else
        {
            // Il y a des messages à afficher
            bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0';

                // Copier les données reçues dans le tampon complet
                if (total_received + bytes_received < sizeof(full_message))
                {
                    memcpy(full_message + total_received, buffer, bytes_received);
                    total_received += bytes_received;
                }
                else
                {
                    fprintf(stderr, "Le tampon de réception est plein, données perdues.\n");
                    break;
                }

                // Vérifier si un message complet a été reçu (en supposant que chaque message se termine par "\n\n")
                if (strstr(full_message, "\n\n") != NULL)
                {
                    // Traiter le message complet
                    if (strncmp(full_message, "200/OK\n", 7) == 0)
                    {
                        memmove(full_message, full_message + 7, strlen(full_message) - 6);
                    }

                    if (!code_to_message(full_message)) {
                        peut_naviguer = 1;
                        printf("%.*s", (int)(total_received), full_message);
                    }

                    // Réinitialiser les tampons pour recevoir un autre message
                    total_received = 0;
                    memset(full_message, 0, sizeof(full_message));
                }
            }
            else if (bytes_received == 0)
            {
                // Le serveur a fermé la connexion
                printf("Le serveur est clos.n");
                break; // Sortir de la boucle
            }
            else if (bytes_received == -1)
            {
                // Autres erreurs de réception
                perror("Erreur de réception des données\n");
                break; // Sortir de la boucle en cas d'erreur
            }
        }
    }
    printf("------------------------------------------\n");

    if (peut_naviguer)
    {
        x_messages_precedents(sock, id_client);
    }
}

void info_message(int sock)
{
    char id_message[50];

    printf("Entrez l'identifiant du message : ");
    fgets(id_message, sizeof(id_message), stdin);
    id_message[strcspn(id_message, "\n")] = 0; // Supprimer le newline à la fin de l'identifiant

    // Construire la requête /info
    char requete[1024];
    snprintf(requete, sizeof(requete), "/info %s", id_message);

    // Envoyer la requête au serveur
    send(sock, requete, strlen(requete), 0);

    // Recevoir la réponse du serveur
    char buffer[1024];
    ssize_t len = recv(sock, buffer, sizeof(buffer), 0);
    buffer[len] = '\0';

    ssize_t bytes_received;
    char full_message[10000];  // Tampon pour accumuler le message complet
    size_t total_received = 0; // Taille totale des données reçues

    while (1)
    {
        // Préparer l'ensemble de descripteurs pour select()
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        // Définir le timeout à 200ms
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000; // 200ms

        // Appel select pour attendre des données ou un timeout
        int ret = select(sock + 1, &readfds, NULL, NULL, &timeout);

        if (ret == -1)
        {
            // En cas d'erreur
            perror("Erreur de select");
            break;
        }
        else if (ret == 0)
        {
            // Plus rien reçu dans les dernières 200ms, sortie de la boucle
            if (total_received > 0)
            {
                if (!code_to_message(full_message) && strcmp(trim_newline(full_message), "204/NO CONTENT") == 0)
                {
                    printf("Vous n'avez aucun nouveau message\n");
                }
                else if (strcmp(trim_newline(full_message), "403/FORBIDDEN") == 0)
                {
                    printf("Votre rôle actuel ne vous permet pas d'avoir des messages\n");
                }
                else if (strcmp(trim_newline(full_message), "416/RANGE NOT SATISFIABLE") == 0)
                {
                    printf("La page que vous demandez n'existe pas\n");
                }
                else if (strcmp(trim_newline(full_message), "500/INTERNAL SERVER ERROR") == 0)
                {
                    printf("Erreur du serveur lors de la lecture de vos messages");
                }
            }
            break;
        }
        else
        {
            // Il y a des données à recevoir
            bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0';

                // Copier les données reçues dans le tampon complet
                if (total_received + bytes_received < sizeof(full_message))
                {
                    memcpy(full_message + total_received, buffer, bytes_received);
                    total_received += bytes_received;
                }
                else
                {
                    fprintf(stderr, "Le tampon de réception est plein, données perdues.\n");
                    break;
                }

                // Vérifier si un message complet a été reçu (en supposant que chaque message se termine par "\n\n")
                if (strstr(full_message, "\n") != NULL)
                {
                    // Traiter le message complet
                    if (strncmp(full_message, "200/OK\n", 7) == 0)
                    {
                        memmove(full_message, full_message + 7, strlen(full_message) - 6);
                    }

                    if(!code_to_message(full_message)) {
                        printf("%.*s", (int)(total_received), full_message);
                    }

                    // Réinitialiser les tampons pour recevoir un autre message
                    total_received = 0;
                    memset(full_message, 0, sizeof(full_message));
                }
            }
            else if (bytes_received == 0)
            {
                // Le serveur a fermé la connexion
                printf("Le serveur a clos la connexion.\n");
                break;
            }
            else if (bytes_received == -1)
            {
                // Autres erreurs de réception
                perror("Erreur de réception des données\n");
                break; // Sortir de la boucle en cas d'erreur
            }
        }
    }
}

void modifier_message(int sock)
{
    char *id_message = malloc(15 * sizeof(char));
    char message[1024];

    printf("Entrez l'id du message : ");
    scanf("%s", id_message);
    getchar(); // Pour consommer le newline laissé par scanf

    printf("Entrez le nouveau message : ");
    fgets(message, sizeof(message), stdin);
    message[strcspn(message, "\n")] = 0; // Supprimer le newline à la fin du message

    // Construire la requête /modifie
    char requete[1024];
    snprintf(requete, sizeof(requete), "/modifie %s %s", id_message, message);

    // Envoyer la requête au serveur
    send(sock, requete, strlen(requete), 0);

    // Recevoir la réponse du serveur
    char buffer[1024];
    int recv_bytes = recv(sock, buffer, sizeof(buffer), 0);
    if (recv_bytes > 0)
    {
        buffer[recv_bytes] = '\0';
        if (!code_to_message(buffer)) {
            printf("Réponse du serveur: %s", buffer);
        }
    }
    else
    {
        printf("Impossible de recevoir la réponse du serveur\n");
    }
    free(id_message);
}

void bloquer_client(int sock)
{
    char *id_client = malloc(15 * sizeof(char));

    printf("Entrez l'id du client : ");
    scanf("%s", id_client);
    getchar(); // Pour consommer le newline laissé par scanf

    // Construire la requête /bloque
    char requete[1024];
    snprintf(requete, sizeof(requete), "/bloque %s", id_client);

    // Envoyer la requête au serveur
    send(sock, requete, strlen(requete), 0);

    // Recevoir la réponse du serveur
    char buffer[1024];
    int recv_bytes = recv(sock, buffer, sizeof(buffer), 0);
    if (recv_bytes > 0)
    {
        buffer[recv_bytes] = '\0';
        if (!code_to_message(buffer)) {
            printf("Réponse du serveur: %s", buffer);
        }
    }
    else
    {
        printf("Impossible de recevoir la réponse du serveur\n");
    }
    free(id_client);
}

void enlever_blocage(int sock)
{
    char *id_client = malloc(15 * sizeof(char));

    printf("Entrez l'id du client : ");
    scanf("%s", id_client);
    getchar(); // Pour consommer le newline laissé par scanf

    // Construire la requête /debloque
    char requete[1024];
    snprintf(requete, sizeof(requete), "/debloque %s", id_client);

    // Envoyer la requête au serveur
    send(sock, requete, strlen(requete), 0);

    // Recevoir la réponse du serveur
    char buffer[1024];
    int recv_bytes = recv(sock, buffer, sizeof(buffer), 0);
    if (recv_bytes > 0)
    {
        buffer[recv_bytes] = '\0';
        if (!code_to_message(buffer)) {
            printf("Réponse du serveur: %s", buffer);
        }
    }
    else
    {
        printf("Impossible de recevoir la réponse du serveur\n");
    }
    free(id_client);
}

void bannir_client(int sock)
{
    char *id_client = malloc(15 * sizeof(char));

    printf("Entrez l'id du client : ");
    scanf("%s", id_client);
    getchar(); // Pour consommer le newline laissé par scanf

    // Construire la requête /ban
    char requete[1024];
    snprintf(requete, sizeof(requete), "/ban %s", id_client);

    // Envoyer la requête au serveur
    send(sock, requete, strlen(requete), 0);

    // Recevoir la réponse du serveur
    char buffer[1024];
    int recv_bytes = recv(sock, buffer, sizeof(buffer), 0);
    if (recv_bytes > 0)
    {
        buffer[recv_bytes] = '\0';
        if (!code_to_message(buffer)) {
            printf("Réponse du serveur: %s", buffer);
        }
    }
    else
    {
        printf("Impossible de recevoir la réponse du serveur\n");
    }
    free(id_client);
}

void enlever_ban(int sock)
{
    char *id_client = malloc(15 * sizeof(char));

    printf("Entrez l'id du client : ");
    scanf("%s", id_client);
    getchar(); // Pour consommer le newline laissé par scanf

    // Construire la requête /deban
    char requete[1024];
    snprintf(requete, sizeof(requete), "/deban %s", id_client);

    // Envoyer la requête au serveur
    send(sock, requete, strlen(requete), 0);

    // Recevoir la réponse du serveur
    char buffer[1024];
    int recv_bytes = recv(sock, buffer, sizeof(buffer), 0);
    if (recv_bytes > 0)
    {
        buffer[recv_bytes] = '\0';
        if (!code_to_message(buffer)) {
            printf("Réponse du serveur: %s", buffer);
        }
    }
    else
    {
        printf("Impossible de recevoir la réponse du serveur\n");
    }
    free(id_client);
}

// Traitement de la commande de l'utilisateur (avec vérification de rôle)
void traiter_commande(int choix, int sock, char *role)
{
    switch (choix)
    {
    case 1:
        if (strcmp(role, "aucun") == 0)
        {
            connexion(sock);
            update_role(sock, role); // Met à jour le rôle après la connexion
        }
        else if (strcmp(role, "membre") == 0 || strcmp(role, "pro") == 0)
        {
            envoyer_message(sock);
        }
        else
        {
            bloquer_client(sock);
        }
        break;
    case 2:
        if (strcmp(role, "aucun") == 0)
        {
            deconnexion(sock);
        }
        else if (strcmp(role, "membre") == 0 || strcmp(role, "pro") == 0)
        {
            messages_non_lus(sock);
        }
        else
        {
            enlever_blocage(sock);
        }
        break;
    case 3:
        if (strcmp(role, "aucun") == 0)
        {
            printf("Option invalide.\n");
        }
        else if (strcmp(role, "membre") == 0 || strcmp(role, "pro") == 0)
        {
            info_message(sock);
        }
        else
        {
            bannir_client(sock);
        }
        break;

    // Options disponibles selon le rôle
    case 4:
        if (strcmp(role, "aucun") == 0)
        {
            printf("Option invalide.\n");
        }
        else if (strcmp(role, "membre") == 0 || strcmp(role, "pro") == 0)
        {
            modifier_message(sock);
        }
        else
        {
            enlever_ban(sock);
        }
        break;

    case 5:
        if (strcmp(role, "aucun") == 0)
        {
            printf("Option invalide.\n");
        }
        else if (strcmp(role, "membre") == 0 || strcmp(role, "pro") == 0)
        {
            supprimer_message(sock);
        }
        else
        {
            synchroniser_params(sock);
        }
        break;
    case 6:
        if (strcmp(role, "aucun") == 0)
        {
            printf("Option invalide.\n");
        }
        else if (strcmp(role, "membre") == 0 || strcmp(role, "pro") == 0)
        {
            historique_message(sock);
        }
        else
        {
            logs(sock);
        }
        break;

    case 7:
        if (strcmp(role, "aucun") == 0)
        {
            printf("Option invalide.\n");
        }
        else if (strcmp(role, "membre") == 0 || strcmp(role, "admin") == 0)
        {
            deconnexion(sock);
        }
        else
        {
            bloquer_client(sock);
        }
        break;

    case 8:
        if (strcmp(role, "admin") == 0)
        {
            fermer_service(sock); // TODO
        }
        else if (strcmp(role, "pro") == 0)
        {
            enlever_blocage(sock);
        }
        else
        {
            printf("Option invalide.\n");
        }
        break;

    case 9:
        if (strcmp(role, "pro") == 0)
        {
            deconnexion(sock);
        }
        else
        {
            printf("Option invalide.\n");
        }
        break;
    default:
        printf("Option invalide.\n");
    }
}

int main(int argc, char *argv[])
{
    // Vérifier si un port est passé en paramètre
    if (argc <= 1)
    {
        printf("Aucun port spécifié\n");
        exit(1);
    }

    int port = atoi(argv[1]);
    if (!port)
    {
        printf("Veuillez saisir un port valide en paramètre");
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
            fflush(stdin);
            continue; // Redemander une entrée
        }
        traiter_commande(choix, sock, role);
    }

    return 0;
}
