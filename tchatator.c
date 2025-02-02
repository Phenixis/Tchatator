#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include <time.h>
#include <pthread.h>

#define _XOPEN_SOURCE

struct param
{
    char *name;
    char *value;
};

// Roles possibles que peut avoir le client en se connectant notamment
typedef enum
{
    AUCUN = 0,
    MEMBRE = 1,
    PRO = 2,
    ADMIN = 3
} Role;
const char *role_to_string(Role role)
{
    switch (role)
    {
    case AUCUN:
        return "aucun";
    case MEMBRE:
        return "membre";
    case PRO:
        return "pro";
    case ADMIN:
        return "admin";
    default:
        return "unknown";
    }
}

int is_positive_integer(const char *str)
{
    if (str == NULL || *str == '\0')
    {
        return 0;
    }
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (!isdigit(str[i]))
        {
            return 0;
        }
    }
    return 1;
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

void read_param_file(struct param *params, char *filename)
{
    FILE *file = fopen(filename, "r");

    if (!file)
    {
        perror("fopen failed");
        exit(1);
    }

    char line[1024];
    int index = 0;

    while (fgets(line, sizeof(line), file) && index < 100)
    {
        char *name = strtok(line, "=");
        char *value = strtok(NULL, "#");
        if (value)
        {
            char *comment = strchr(value, '#');
            if (comment)
            {
                *comment = '\0';
            }
        }

        if (name && value)
        {

            params[index].name = trim_newline(name);
            params[index].value = trim_newline(value);
            index++;
        }
    }

    for (int i = index; i < 100; i++)
    {
        params[i].name = NULL;
        params[i].value = NULL;
    }

    fclose(file);
}

char *get_param(struct param *params, const char *name)
{
    for (int i = 0; i < 100; i++)
    {
        if (params[i].name == NULL || params[i].value == NULL)
        {
            break;
        }
        else if (strcmp(params[i].name, name) == 0)
        {
            return params[i].value;
        }
    }
    printf("Paramètre [%s] introuvable dans le fichier de paramétrage\n", name);
    exit(1);
}

PGresult *execute(PGconn *conn, char *query)
{
    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Query failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        exit(EXIT_FAILURE);
    }
    return res;
}

int logs(char *message, char *clientID, char *clientIP, int verbose)
{
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%d/%m/%Y - %H:%M:%S", local);
    FILE *file = fopen("tchatator.log", "a");
    if (!file)
    {
        perror("fopen failed on tchatator.log");
        return 1;
    }
    if (verbose == 1)
    {
        printf("[date+heure:%s] [id_client:%s] [ip_client:%s] : %s\n", time_str, clientID, clientIP, message);
    }
    fprintf(file, "[date+heure:%s] [id_client:%s] [ip_client:%s] : %s\n", time_str, clientID, clientIP, message);
    fclose(file);
    return 0;
}

// Assume get_param() and logs() are already defined elsewhere.
char send_answer(int cnx, struct param *params, char *code, char *clientID, char *clientIP, int verbose)
{
    char *value = get_param(params, code);

    if (value)
    {
        // Donner la bonne longueur à message
        int message_length = strlen(code) + strlen(value) + 2; // 1 pour le '/' et 1 pour '\0'
        char *message = malloc(message_length);

        if (!message)
        {
            perror("Failed to allocate memory for message");
            return 0;
        }

        // Construire dynamiquement le message
        snprintf(message, message_length, "%s/%s", code, value);

        // Log le message
        char *to_log = malloc(strlen(message) + 100);
        if (to_log)
        {
            snprintf(to_log, strlen(message) + 100, "Réponse envoyée : %s", message);
            logs(to_log, clientID, clientIP, verbose);
            free(to_log);
        }
        else
        {
            perror("Failed to allocate memory for logging");
            free(message);
            return 0;
        }

        // Add the newline character to the message
        char *message_with_newline = malloc(strlen(message) + 2); // +1 for newline, +1 for null terminator
        if (message_with_newline)
        {
            snprintf(message_with_newline, strlen(message) + 2, "%s\n", message);
            write(cnx, message_with_newline, strlen(message_with_newline));
            free(message_with_newline);
        }
        else
        {
            perror("Failed to allocate memory for message with newline");
            free(message);
            return 0;
        }

        free(message);

        return 1;
    }
    else
    {
        // Aucune valeur trouvé pour le code donné
        return send_answer(cnx, params, "500", clientID, clientIP, verbose);
    }
}

char send_nb_non_lus(int cnx, PGconn *conn, char *clientID, char *clientIP, int verbose)
{
    int result;
    char query[256];

    if (strcmp(clientID, "") != 0)
    {
        snprintf(query, sizeof(query), "SELECT count FROM sae_db.vue_nb_messages_non_lus WHERE id_receveur = '%s';", clientID);
        PGresult *res = execute(conn, query);
        if (PQntuples(res) > 0)
        {
            result = atoi(PQgetvalue(res, 0, 0));
        }
        else
        {
            result = 0;
        }
        PQclear(res);
    }
    else
    {
        result = 0;
    }

    // Log le message
    size_t log_len = sizeof(int) + 100;
    char *to_log = malloc(log_len);

    if (to_log)
    {
        // Créer le log
        snprintf(to_log, log_len, "Réponse envoyée : %d", result);
        logs(to_log, clientID, clientIP, verbose);

        // Envoi du rôle
        ssize_t bytes_sent = write(cnx, &result, sizeof(int));

        if (bytes_sent == -1)
        {
            // Gérer l'erreur d'envoi
            perror("Erreur lors de l'envoi du nombre de messages non lus");
            free(to_log);
            return 0;
        }

        // Si l'envoi a réussi, on libère la mémoire allouée pour le log
        free(to_log);
    }
    else
    {
        perror("Échec de l'allocation de mémoire pour le log");
        return -1;
    }

    return 1; // Rôle envoyé avec succès
}

char send_role(int cnx, Role role, char *clientID, char *clientIP, int verbose)
{
    // Convertir le rôle en chaîne de caractères
    const char *string_role = role_to_string(role);

    // Log le message
    size_t log_len = strlen(string_role) + 100;
    char *to_log = malloc(log_len);

    if (to_log)
    {
        // Créer le log
        snprintf(to_log, log_len, "Réponse envoyée : %s", string_role);
        logs(to_log, clientID, clientIP, verbose);

        // Envoi du rôle
        ssize_t bytes_sent = write(cnx, string_role, strlen(string_role));

        if (bytes_sent == -1)
        {
            // Gérer l'erreur d'envoi
            perror("Erreur lors de l'envoi du rôle");
            free(to_log);
            return 0;
        }

        // Si l'envoi a réussi, on libère la mémoire allouée pour le log
        free(to_log);
    }
    else
    {
        perror("Échec de l'allocation de mémoire pour le log");
        return -1;
    }

    return 1; // Rôle envoyé avec succès
}

char send_messages_non_lus(PGconn *conn, int cnx, PGresult *res, char *clientID, char *clientIP, int verbose)
{
    int rows = PQntuples(res); // Nombre de lignes dans le résultat
    int i;

    // On parcourt chaque ligne du résultat
    for (i = 0; i < rows; i++)
    {
        // Récupérer les valeurs des colonnes
        const char *id_message = PQgetvalue(res, i, 0);
        const char *mail_envoyeur = PQgetvalue(res, i, 1);
        const char *message = PQgetvalue(res, i, 2);
        const char *date_envoi = PQgetvalue(res, i, 3);

        // Vérifier que les valeurs ne sont pas NULL
        if (mail_envoyeur == NULL || date_envoi == NULL || message == NULL)
        {
            fprintf(stderr, "Une des valeurs extraites est NULL pour la ligne %d\n", i);
            continue; // Passer à la prochaine ligne
        }

        // Formater le message à envoyer
        char buffer[10000]; // Assurer que la taille du buffer est suffisante pour le message formaté
        int n = snprintf(buffer, sizeof(buffer), "%s\t\t%s\n%s\n\n", mail_envoyeur, date_envoi, message);

        // Vérifier si la taille du message dépasse la taille du buffer
        if (n >= sizeof(buffer))
        {
            fprintf(stderr, "Le message est trop long pour le buffer\n");
            return -1;
        }

        // Envoyer via la socket
        size_t total_sent = 0; // Total des bytes envoyés
        ssize_t sent_bytes;
        size_t buffer_len = strlen(buffer);

        while (total_sent < buffer_len)
        {
            sent_bytes = write(cnx, buffer + total_sent, buffer_len - total_sent);
            if (sent_bytes == -1)
            {
                perror("Erreur d'envoi sur la socket");
                return -1; // Erreur lors de l'envoi
            }
            else
            {
                // Marquer les messages comme lus dans la base de données
                char query[256];
                snprintf(query, sizeof(query), "UPDATE sae_db._message SET date_lecture = NOW() WHERE id='%s';", id_message);
                execute(conn, query);
                total_sent += sent_bytes; // Mettre à jour le nombre total de bytes envoyés
            }
        }
        size_t log_len = strlen(buffer) + 100;
        char *to_log = malloc(log_len);

        if (to_log)
        {
            // Créer le log
            snprintf(to_log, log_len, "Réponse envoyée : %s", trim_newline(buffer));
            logs(to_log, clientID, clientIP, verbose);
            free(to_log);
        }
        else
        {
            perror("Échec de l'allocation de mémoire pour le log");
            return -1;
        }
    }

    return 1; // Succès
}

char send_historique(int cnx, PGresult *res, char *clientID, char *clientIP, int verbose)
{
    int rows = PQntuples(res); // Nombre de lignes dans le résultat
    int i;

    // On parcourt chaque ligne du résultat
    for (i = 0; i < rows; i++)
    {
        // Récupérer les valeurs des colonnes
        const char *id_message = PQgetvalue(res, i, 0);
        const char *mail_envoyeur = PQgetvalue(res, i, 1);
        const char *mail_receveur = PQgetvalue(res, i, 2);
        const char *message = PQgetvalue(res, i, 3);
        const char *date_envoi = PQgetvalue(res, i, 4);

        // Vérifier que les valeurs ne sont pas NULL
        if (id_message == NULL || mail_envoyeur == NULL || mail_receveur == NULL || date_envoi == NULL || message == NULL)
        {
            fprintf(stderr, "Une des valeurs extraites est NULL pour la ligne %d\n", i);
            continue;
        }

        // Formater le message à envoyer
        char buffer[10000]; // Assurer que la taille du buffer est suffisante pour le message formaté
        int n = snprintf(buffer, sizeof(buffer), "[%s] %s->%s\t\t%s\n%s\n\n", id_message, mail_envoyeur, mail_receveur, date_envoi, message);

        // Vérifier si la taille du message dépasse la taille du buffer
        if (n >= sizeof(buffer))
        {
            fprintf(stderr, "Le message est trop long pour le buffer\n");
            return -1;
        }

        // Envoyer via la socket
        size_t total_sent = 0;
        ssize_t sent_bytes;
        size_t buffer_len = strlen(buffer);

        while (total_sent < buffer_len)
        {
            sent_bytes = write(cnx, buffer + total_sent, buffer_len - total_sent);
            if (sent_bytes == -1)
            {
                perror("Erreur d'envoi sur la socket");
                return -1; // Erreur lors de l'envoi
            }
            total_sent += sent_bytes; // Mettre à jour le nombre total de bytes envoyés
        }
        size_t log_len = strlen(buffer) + 100;
        char *to_log = malloc(log_len);

        if (to_log)
        {
            // Créer le log
            snprintf(to_log, log_len, "Réponse envoyée : %s", trim_newline(buffer));
            logs(to_log, clientID, clientIP, verbose);
            free(to_log);
        }
        else
        {
            perror("Échec de l'allocation de mémoire pour le log");
            return -1;
        }
    }
    return 1; // Succès
}

void exit_on_error(PGconn *conn)
{
    fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
    PQfinish(conn);
    exit(EXIT_FAILURE);
}

PGconn *get_connection(struct param *params, int verbose)
{
    char *host = get_param(params, "db_host");
    char *port = get_param(params, "db_port");
    char *dbname = get_param(params, "db_name");
    char *user = get_param(params, "db_user");
    char *password = get_param(params, "db_password");
    char conninfo[1024];
    sprintf(conninfo, "host=%s port=%s dbname=%s user=%s password=%s", host, port, dbname, user, password);

    // Connect to the database
    PGconn *conn = PQconnectdb(conninfo);

    // Check if the connection was successful
    if (PQstatus(conn) != CONNECTION_OK)
    {
        exit_on_error(conn);
    }

    logs("Successfully connected to the database!", "", "", verbose);

    return conn;
}

/*
Renvoie 0 si le client n'existe pas, 1 ou plus sinon
*/
int client_existe(PGconn *conn, char *id_client)
{
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM sae_db._compte WHERE id_compte = '%s';", id_client);
    PGresult *res = execute(conn, query);
    int result = PQntuples(res);
    PQclear(res);
    return result;
}
/* Renvoie 0 si le message n'existe pas, 1 ou plus sinon*/
int message_existe(PGconn *conn, char *id_message)
{
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM sae_db._message WHERE id = '%s';", id_message);
    PGresult *res = execute(conn, query);
    int result = PQntuples(res);
    PQclear(res);
    return result;
}
/* Renvoie 0 si le client n'est pas l'envoyeur, 1 ou plus sinon */
int client_est_envoyeur(PGconn *conn, char *id_client, char *id_message)
{
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM sae_db._message WHERE id = '%s' AND id_envoyeur = '%s';", id_message, id_client);
    PGresult *res = execute(conn, query);
    int result = PQntuples(res);
    PQclear(res);
    return result;
}
/*
Renvoie 0 si le client n'est pas membre, 1 ou plus sinon
*/
int client_est_membre(PGconn *conn, char *id_client)
{
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM sae_db._membre WHERE id_compte = '%s';", id_client);
    PGresult *res = execute(conn, query);
    int result = PQntuples(res);
    PQclear(res);
    return result;
}

/*
Renovie 0 si le client n'est pas professionnel, 1 ou plus sinon
*/
int client_est_pro(PGconn *conn, char *id_client)
{
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM sae_db._professionnel WHERE id_compte = '%s';", id_client);
    PGresult *res = execute(conn, query);
    int result = PQntuples(res);
    PQclear(res);
    return result;
}

/*
Renvoie 0 si le client n'est pas banni, 1 ou plus sinon
*/
int client_est_banni(PGconn *conn, char *id_client)
{
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM sae_db._bannissement WHERE id_banni = '%s' AND (date_debannissement IS NULL OR date_debannissement > NOW());", id_client);
    PGresult *res = execute(conn, query);
    int result = PQntuples(res) > 0;
    PQclear(res);
    return result;
}

/*
Renvoie 0 si le client n'est pas bloqué, 1 ou plus sinon
*/
int client_est_bloque(PGconn *conn, char *id_client, char *id_bloqueur)
{
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM sae_db._blocage WHERE id_bloque = '%s' AND (id_bloqueur = '%s' OR id_bloqueur = '-1') AND date_deblocage > NOW();", id_client, id_bloqueur);
    PGresult *res = execute(conn, query);
    int result = PQntuples(res);
    PQclear(res);
    return result;
}

int compteur_minute = 0;
int compteur_heure = 0;
pthread_mutex_t compteur_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex pour protéger le compteur

// Fonction pour réinitialiser le compteur toutes les heures
void* reset_heure(void* arg) {
    time_t start_time, current_time;
    start_time = time(NULL);

    while (1) {
        current_time = time(NULL);
        
        // Vérifier si une minute est écoulée
        if (difftime(current_time, start_time) >= 3600) {
            pthread_mutex_lock(&compteur_mutex);  // Verrouiller l'accès au compteur
            compteur_minute = 0;  // Réinitialiser le compteur
            pthread_mutex_unlock(&compteur_mutex);  // Libérer l'accès au compteur

            // Réinitialiser le temps de départ
            start_time = current_time;
        }

        // Petite pause pour ne pas trop utiliser le processeur
        sleep(1);
    }

    return NULL;
}

// Fonction pour réinitialiser le compteur toutes les minutes
void* reset_minute(void* arg) {
    time_t start_time, current_time;
    start_time = time(NULL);

    while (1) {
        current_time = time(NULL);
        
        // Vérifier si une minute est écoulée
        if (difftime(current_time, start_time) >= 60) {
            pthread_mutex_lock(&compteur_mutex);  // Verrouiller l'accès au compteur
            compteur_minute = 0;  // Réinitialiser le compteur
            pthread_mutex_unlock(&compteur_mutex);  // Libérer l'accès au compteur

            // Réinitialiser le temps de départ
            start_time = current_time;
        }

        // Petite pause pour ne pas trop utiliser le processeur
        sleep(1);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t thread_minute, thread_heure;

    // Créer un thread pour réinitialiser le compteur
    if (pthread_create(&thread_minute, NULL, reset_minute, NULL) != 0) {
        perror("Erreur de création de thread");
        return 1;
    }
    // Créer un thread pour réinitialiser le compteur des heures
    if (pthread_create(&thread_heure, NULL, reset_heure, NULL) != 0) {
        perror("Erreur de création de thread pour les heures");
        return 1;
    }

    char *param_file = ".tchatator"; // Fichier de paramètres par défaut
    int verbose = 0;
    Role role = AUCUN;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fileparam") == 0)
        {
            if (i + 1 < argc)
            {
                param_file = argv[i + 1];
                i++; // Skip the next argument as it is the file name
            }
            else
            {
                printf("Missing argument for option %s\n", argv[i]);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
        {
            verbose = 1;
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            printf("Usage: %s [-f|--fileparam FILE] [-v|--verbose]\n", argv[0]);
            printf("Options:\n");
            printf("  -f, --fileparam FILE  Use FILE as the parameter file (default: .tchatator)\n");
            printf("  -v, --verbose         Print verbose output\n");
            return 0;
        }
        else
        {
            printf("Unknown option: %s\n", argv[i]);
            return 1;
        }
    }
    struct param params[100];
    read_param_file(params, param_file);

    char *to_log = malloc(200);
    sprintf(to_log, "Service lancé avec le fichier de paramétrage '%s' et le mode verbose '%d'", param_file, verbose);
    logs(to_log, "", "", verbose);

    int sock;
    struct sockaddr_in addr;
    int ret;
    char *portName = get_param(params, "socket_port");
    int limite_minute = atoi(get_param(params, "max_requests_per_minute"));
    int limite_heure = atoi(get_param(params, "max_requests_per_hour"));
    int port = atoi(portName);
    int port_max = port + 10;
    int size;
    int cnx;
    struct sockaddr_in conn_addr;
    char *taille_bloc = get_param(params, "taille_bloc");

    // Création de la socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("Création du socket échouée : ");
        return 1;
    }
    sprintf(to_log, "Socket: %d", sock);
    logs(to_log, "", "", verbose);

    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_family = AF_INET;

    // Cette boucle permet de trouver un port libre
    while (1)
    {
        addr.sin_port = htons(port);
        ret = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
        if (ret == -1)
        {
            if (errno == EADDRINUSE)
            {

                sprintf(to_log, "Le port %d est déjà utilisé, essaie pour le port %d", port, port + 1);
                logs(to_log, "", "", 1);
                if (port > port_max)
                {
                    perror("Aucun port disponible");
                    close(sock);
                    return 1;
                }
                port++;
                continue;
            }
            else
            {
                perror("Erreur lors du bind");
                close(sock);
                return 1;
            }
        }
        break;
    }
    sprintf(to_log, "Socket bind sur le port %d", port);
    logs(to_log, "", "", 1);

    // Mise en écoute de la socket
    char *max_pending_connections = get_param(params, "max_pending_connections");
    ret = listen(sock, atoi(max_pending_connections));
    if (ret == -1)
    {
        perror("Listen failed");
        close(sock);
        return 1;
    }
    sprintf(to_log, "Listen: %d", ret);
    logs(to_log, "", "", verbose);

    int go_on = 0;
    while (go_on == 0)
    {
        size = sizeof(conn_addr);
        cnx = accept(sock, (struct sockaddr *)&conn_addr, (socklen_t *)&size);
        if (cnx == -1)
        {
            perror("Erreur lors de l'acceptation de la connexion");
            close(sock);
            return 1;
        }
        sprintf(to_log, "Accept: %d", cnx);
        logs(to_log, "", "", verbose);

        char *client_ip = inet_ntoa(conn_addr.sin_addr);
        PGconn *conn = get_connection(params, verbose);
        char buffer[2048];
        char id_compte_client[50];
        int len;
        char *trimmed_buffer;

        do
        {
            free(trimmed_buffer);
            len = read(cnx, buffer, sizeof(buffer) - 1);
            buffer[len] = '\0';
            trimmed_buffer = trim_newline(buffer);

            // Afficher le message reçu
            char *to_log = malloc(strlen(trimmed_buffer) + 100);
            sprintf(to_log, "Message reçu : %s", trimmed_buffer);
            logs(to_log, id_compte_client, client_ip, verbose);

            // Incrémenter les compteurs de requête
            compteur_heure ++;
            compteur_minute ++;

            // ###############
            // # EXTRA UTILS #
            // ###############
            if (strncmp(trimmed_buffer, "/nb_non_lus", 11) == 0)
            {
                logs("Commande /nb_non_lus", id_compte_client, client_ip, verbose);
                send_nb_non_lus(cnx, conn, id_compte_client, client_ip, verbose);
            }
            else if (strncmp(trimmed_buffer, "/role", 5) == 0)
            {
                logs("Commande /role", id_compte_client, client_ip, verbose);
                send_role(cnx, role, id_compte_client, client_ip, verbose);
            }

            // ###########################
            // # CONNEXION & DECONNEXION #
            // ###########################
            else if (strncmp(trimmed_buffer, "/deconnexion", 12) == 0)
            {
                if (strcmp(trimmed_buffer, "/deconnexion -h") == 0 || strcmp(trimmed_buffer, "/deconnexion --help") == 0)
                {
                    logs("Commande d'aide /deconnexion", id_compte_client, client_ip, verbose);
                    char *help_message = "Usage: /deconnexion\nDéconnecte le client et ferme la connexion\n";
                    write(cnx, help_message, strlen(help_message));
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else
                {
                    logs("Commande /deconnexion", id_compte_client, client_ip, verbose);
                    strcpy(id_compte_client, "");
                    role = AUCUN;
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                    break;
                }
            }
            else if (strcmp(trimmed_buffer, "/quit") == 0) // Commande ADMIN pour arrêter le Tchatator
            {
                if (strcmp(id_compte_client, "admin") == 0)
                {
                    logs("Arrêt du Tchatator.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                    go_on = 1;
                    break;
                }
                else
                {
                    logs("Le client n'est pas connecté en tant qu'admin.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "404", id_compte_client, client_ip, verbose); // Si le client n'est pas admin, on fait comme si la commande n'existait pas
                }
            }
            // Vérification : ne pas dépasser les limites de requêtes par minute / heure
            else if (compteur_minute > limite_minute) {
                sprintf(to_log, "Limite de requêtes dépassée à la minute ! [%d]", limite_minute);
                logs(to_log, id_compte_client, client_ip, verbose);
                send_answer(cnx, params, "429", id_compte_client, client_ip, verbose);
                continue;;
            }
            else if (compteur_heure > limite_heure) {
                sprintf(to_log, "Limite de requêtes dépassée à l'heure ! [%d]", limite_heure);
                logs(to_log, id_compte_client, client_ip, verbose);
                send_answer(cnx, params, "430", id_compte_client, client_ip, verbose);
                continue;;
            }
            else if (strncmp(trimmed_buffer, "/connexion ", 10) == 0)
            {
                if (strcmp(trimmed_buffer, "/connexion -h") == 0 || strcmp(trimmed_buffer, "/connexion --help") == 0)
                {
                    logs("Commande d'aide /connexion", id_compte_client, client_ip, verbose);
                    write(cnx, "Usage: /connexion {API_KEY}\nConnecte au compte du client avec la clé d'API {API_KEY}.\n", 88);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else if (strncmp(trimmed_buffer, "/connexion tchatator_", 21) == 0)
                {
                    char *api_key = trimmed_buffer + 11;
                    char *admin_api_key = get_param(params, "admin_api_key");
                    char query[512];

                    // Essayer de se connecter en tant que membre
                    snprintf(query, sizeof(query), "SELECT * FROM sae_db._compte WHERE api_key = '%s';", api_key);
                    PGresult *res = execute(conn, query);

                    // Se connecter en tant que membre ou pro
                    if (PQntuples(res) > 0)
                    {
                        logs("Le client a fourni une clé d'API reconnue.", id_compte_client, client_ip, verbose);
                        strcpy(id_compte_client, PQgetvalue(res, 0, 0));

                        if (client_est_banni(conn, id_compte_client) == 1) // Interdit de se connecter si le client est banni
                        {
                            logs("Le client est banni. Connexion refusée.", id_compte_client, client_ip, verbose);
                            send_answer(cnx, params, "403", id_compte_client, client_ip, verbose);
                            strcpy(id_compte_client, "");
                            continue;
                        }

                        if (client_est_membre(conn, id_compte_client) > 0)
                        {
                            role = MEMBRE;
                        }
                        else
                        {
                            role = PRO;
                        }

                        char update_query[256];
                        snprintf(update_query, sizeof(update_query), "UPDATE sae_db._compte SET derniere_connexion = NOW() WHERE id_compte = '%s';", id_compte_client);
                        execute(conn, update_query);
                        logs("Connexion réussie.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);

                        PQclear(res);
                    }
                    // Se connecter en tant qu'admin
                    else if (strcmp(api_key, admin_api_key) == 0)
                    {
                        logs("Le client a fourni une clé d'API admin. Connexion réussie.", id_compte_client, client_ip, verbose);
                        strcpy(id_compte_client, "admin");
                        role = ADMIN;
                        send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                    }
                    else
                    {
                        logs("Le client a fourni une clé d'API inconnue. Connexion refusée.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "401", id_compte_client, client_ip, verbose);
                    }
                }
                else
                {
                    logs("Commande invalide.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                }
            }

            // ##############
            // # MESSAGERIE #
            // ##############
            else if (strncmp(trimmed_buffer, "/message ", 9) == 0)
            {
                if (strcmp(trimmed_buffer, "/message -h") == 0 || strcmp(trimmed_buffer, "/message --help") == 0)
                {
                    logs("Commande d'aide /message", id_compte_client, client_ip, verbose);
                    char *help_message = "Usage: /message {id_compte} {message}\nEnvoie un message au compte spécifié.\n";
                    write(cnx, help_message, strlen(help_message));
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else if (client_est_membre(conn, id_compte_client) > 0 || client_est_pro(conn, id_compte_client) > 0)
                {
                    logs("Le client est connecté en tant que membre ou professionnel.", id_compte_client, client_ip, verbose);
                    char *id_receveur = strtok(trimmed_buffer + 9, " ");
                    char *message = strtok(NULL, "");

                    if (client_existe(conn, id_receveur) == 0 || client_est_banni(conn, id_receveur) >= 1) // Si le client est banni, on le considère comme "inexistant"
                    {
                        logs("Le receveur n'existe pas ou est banni.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    // Si receveur et envoyeur sont tous les deux des membres ou des professionnels, on refuse l'envoi
                    if ((client_est_pro(conn, id_compte_client) > 0 && client_est_pro(conn, id_receveur) > 0) || (client_est_membre(conn, id_compte_client) > 0 && client_est_membre(conn, id_receveur) > 0))
                    {
                        logs("Le client ne peut pas envoyer de message à un autre professionnel ou membre.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "409", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    if (client_est_bloque(conn, id_compte_client, id_receveur) > 0 || client_est_bloque(conn, id_receveur, id_compte_client) > 0) // si je suis bloqué par le receveur ou si je bloque le receveur
                    {
                        logs("Le client est bloqué par le receveur ou bloque le receveur.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "403", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    int taille_maxi = atoi(get_param(params, "max_message_size"));

                    if (strlen(message) > taille_maxi)
                    {
                        logs("Le message est trop long.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "413", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    char query[taille_maxi + 256];
                    snprintf(query, sizeof(query), "INSERT INTO sae_db._message (id_envoyeur, id_receveur, message) VALUES ('%s', '%s', '%s');", id_compte_client, id_receveur, message);
                    execute(conn, query);

                    logs("Message envoyé.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else
                {
                    logs("Le client n'est pas connecté en tant que membre ou professionnel ou a rentré une commande invalide.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "401", id_compte_client, client_ip, verbose);
                }
            }
            else if (strncmp(trimmed_buffer, "/liste", 6) == 0)
            {
                if (strcmp(trimmed_buffer, "/liste -h") == 0 || strcmp(trimmed_buffer, "/liste --help") == 0)
                {
                    logs("Commande d'aide /liste", id_compte_client, client_ip, verbose);
                    write(cnx, "Usage: /liste {page=0}\nAffiche la liste de vos messages non lus.\n", 63);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                // Si pas connecté ou admin, aucun message non lu (no content)
                else if (strcmp(id_compte_client, "") == 0 || strcmp(id_compte_client, "admin") == 0)
                {
                    logs("Le client n'est pas connecté ou est un admin.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "204", id_compte_client, client_ip, verbose);
                }
                else
                {
                    // Prendre en compte le bloc donné s'il y en a un
                    char *bloc_param = strstr(trimmed_buffer, "?page=");
                    char *bloc = "0";
                    if (bloc_param != NULL && strcmp(bloc_param + 6, "") != 0)
                    {
                        bloc = bloc_param + 6; // `?page=` fait 6 caractères, on saute cette partie
                    }
                    int offset = atoi(bloc) * atoi(taille_bloc);
                    if (offset < 0) {
                        offset = 0;
                    }

                    // Regarder s'il y a des messages dans la boîte de messages non lus
                    char query[256];
                    snprintf(query, sizeof(query), "SELECT id, email_envoyeur, message, date_envoi_affichee FROM sae_db.vue_messages_non_lus WHERE id_receveur = '%s' OFFSET '%d' LIMIT '%s';", id_compte_client, offset, taille_bloc);
                    PGresult *res = execute(conn, query);

                    // Cas 1 : il y a des messages non lus dans sa boîte
                    if (PQntuples(res) > 0)
                    {
                        // Tout s'est bien passé
                        logs("Messages non lus envoyés.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                        send_messages_non_lus(conn, cnx, res, id_compte_client, client_ip, verbose);
                    }
                    // Cas 2 : il n'y a aucun message
                    else
                    {
                        if (atoi(bloc) <= 0)
                        {
                            logs("Pas de messages non lus.", id_compte_client, client_ip, verbose);
                            send_answer(cnx, params, "204", id_compte_client, client_ip, verbose);
                        }
                        else
                        {
                            logs("Page invalide.", id_compte_client, client_ip, verbose);
                            send_answer(cnx, params, "416", id_compte_client, client_ip, verbose);
                        }
                    }
                }
            }
            else if (strncmp(trimmed_buffer, "/conversation ", 14) == 0)
            {
                if (strcmp(trimmed_buffer, "/conversation -h") == 0 || strcmp(trimmed_buffer, "/conversation --help") == 0)
                {
                    logs("Commande d'aide /conversation", id_compte_client, client_ip, verbose);
                    write(cnx, "Usage: /conversation {id_client} {?page=0}\nAffiche l'historique des messages avec le client spécifié.\n", 105);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                // Ni pro ni membre
                else if (role != MEMBRE && role != PRO)
                {
                    logs("Le client n'est pas connecté en tant que membre ou professionnel.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "403", id_compte_client, client_ip, verbose);
                }
                else
                {
                    // Extraire l'id_client
                    char *id_client_start = trimmed_buffer + 14;
                    char *space_pos = strchr(id_client_start, ' ');

                    char id_client[10];
                    if (space_pos != NULL) {
                        strncpy(id_client, id_client_start, space_pos - id_client_start);
                        id_client[space_pos - id_client_start] = '\0'; // Terminer la chaîne
                    } else {
                        // Si pas d'espace, alors l'ID client est jusqu'à la fin
                        strcpy(id_client, id_client_start);
                    }

                    if (!is_positive_integer(id_client)) {
                        send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                        continue;
                    }
                    
                    char query[2048];
                    snprintf(query, sizeof(query), "SELECT id_compte FROM sae_db._membre WHERE id_compte = '%s' UNION SELECT id_compte FROM sae_db._professionnel WHERE id_compte = '%s';", id_client, id_client);
                    PGresult *res = execute(conn, query);
                    if (PQntuples(res) <= 0)
                    {
                        logs("Le client spécifié n'existe pas.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                    }
                    // Le client spécifié existe
                    else
                    {
                        // Prendre en compte le bloc donné s'il y en a un
                        char *bloc_param = strstr(trimmed_buffer, "?page=");
                        char *bloc = "0";
                        if (bloc_param != NULL && strcmp(bloc_param + 6, "") != 0)
                        {
                            bloc = bloc_param + 6; // `?page=` fait 6 caractères, on saute cette partie
                        }
                        int offset = atoi(bloc) * atoi(taille_bloc);
                        if (offset < 0) {
                            offset = 0;
                        }
                        
                        // Obtenir les messages avec ce client spécifique
                        snprintf(query, sizeof(query), "SELECT id, email_envoyeur, email_receveur, message, date_envoi_affichee FROM sae_db.vue_historique_message WHERE (id_envoyeur = '%s' AND id_receveur = '%s') OR (id_envoyeur = '%s' AND id_receveur = '%s') OFFSET '%d' LIMIT '%s';", id_compte_client, id_client, id_client, id_compte_client, offset, taille_bloc);
                        res = execute(conn, query);

                        // Pas d'obtention de messages
                        if (PQntuples(res) <= 0)
                        {
                            if (atoi(bloc) <= 0)
                            {
                                logs("Pas de messages avec ce client.", id_compte_client, client_ip, verbose);
                                send_answer(cnx, params, "204", id_compte_client, client_ip, verbose);
                            }
                            else
                            {
                                logs("Page invalide.", id_compte_client, client_ip, verbose);
                                send_answer(cnx, params, "416", id_compte_client, client_ip, verbose);
                            }
                        }
                        // On obtient des messages
                        else
                        {
                            // Tout s'est bien passé
                            logs("Historique des messages envoyé.", id_compte_client, client_ip, verbose);
                            send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                            send_historique(cnx, res, id_compte_client, client_ip, verbose);
                        }
                    }
                }
            }
            else if (strncmp(trimmed_buffer, "/precedent", 10) == 0) {
                if (strcmp(trimmed_buffer, "/precedent -h") == 0) {
                    logs("Commande d'aide /precedent", id_compte_client, client_ip, verbose);
                    write(cnx, "Usage: /precedent {id_client} {id_message}\nAffiche les messages précédant le message spécifié de la conversation avec le client spécifié.\n", 75);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                // Ni pro ni membre
                else if (role != MEMBRE && role != PRO)
                {
                    logs("Le client n'est pas connecté en tant que membre ou professionnel.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "403", id_compte_client, client_ip, verbose);
                }
                else
                {
                    char id_client[10];
                    char id_message[10];
                    if (sscanf(buffer, "/precedent %9s %9s", id_client, id_message) != 2 || !is_positive_integer(id_client) || !is_positive_integer(id_message)) {
                        send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    char query[1024];
                    snprintf(query, sizeof(query), "SELECT id_compte FROM sae_db._membre WHERE id_compte = '%s' UNION SELECT id_compte FROM sae_db._professionnel WHERE id_compte = '%s';", id_client, id_client);
                    PGresult *res = execute(conn, query);
                    if (PQntuples(res) <= 0)
                    {
                        send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                    }
                    // Le client spécifié existe
                    else
                    {
                        // Obtenir les messages avec le client + message spécifiés
                        snprintf(query, sizeof(query)," SELECT id, email_envoyeur, email_receveur, message, date_envoi_affichee"
                                                      " FROM sae_db.vue_historique_message"
                                                      " WHERE date_envoi < ("
                                                      "      SELECT date_envoi"
                                                      "      FROM sae_db.vue_historique_message"
                                                      "      WHERE id = '%s'"
                                                      "      AND ("
                                                      "         (id_envoyeur = '%s' AND id_receveur = '%s') OR "
                                                      "         (id_envoyeur = '%s' AND id_receveur = '%s')"
                                                      "      )"
                                                      "  )"
                                                      "  AND ("
                                                      "      (id_envoyeur = '%s' AND id_receveur = '%s') OR "
                                                      "      (id_envoyeur = '%s' AND id_receveur = '%s')"
                                                      "  )"
                                                      "  ORDER BY date_envoi DESC"
                                                      "  LIMIT '%s';"
                        , id_message, id_compte_client, id_client, id_client, id_compte_client, id_compte_client, id_client, id_client, id_compte_client, taille_bloc);
                        res = execute(conn, query);

                        // Pas d'obtention de messages
                        if (PQntuples(res) <= 0)
                        {
                            logs("Pas de messages précédents.", id_compte_client, client_ip, verbose);
                            send_answer(cnx, params, "204", id_compte_client, client_ip, verbose);
                        }
                        // On obtient des messages
                        else
                        {
                            // Tout s'est bien passé
                            logs("Messages précédents envoyés.", id_compte_client, client_ip, verbose);
                            send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                            send_historique(cnx, res, id_compte_client, client_ip, verbose);
                        }
                    }
                }
            }
            else if (strncmp(trimmed_buffer, "/info ", 6) == 0)
            {
                if (strcmp(trimmed_buffer, "/info -h") == 0 || strcmp(trimmed_buffer, "/info --help") == 0)
                {
                    logs("Commande d'aide /info", id_compte_client, client_ip, verbose);
                    write(cnx, "Usage: /info {id_message}\nAffiche les informations du message spécifié.\n", 75);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else if (client_est_membre(conn, id_compte_client) > 0 || client_est_pro(conn, id_compte_client) > 0)
                {
                    logs("Le client est connecté en tant que membre ou professionnel.", id_compte_client, client_ip, verbose);
                    char *id_message = trimmed_buffer + 6;
                    char query[256];
                    snprintf(query, sizeof(query), "SELECT * FROM sae_db._message WHERE id = '%s';", id_message);
                    PGresult *res = execute(conn, query);

                    if (PQntuples(res) == 0)
                    {
                        logs("Le message n'existe pas.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                    }
                    else
                    {
                        char *id_envoyeur = PQgetvalue(res, 0, 1);
                        char *id_receveur = PQgetvalue(res, 0, 2);
                        char *message = PQgetvalue(res, 0, 3);
                        char *date_envoi = PQgetvalue(res, 0, 4);
                        char *date_modification = PQgetvalue(res, 0, 5);
                        char *date_lecture = PQgetvalue(res, 0, 7);
                        struct tm tm;
                        char formatted_date_envoi[30];
                        char formatted_date_modification[30];
                        char formatted_date_lecture[30];

                        if (strptime(date_envoi, "%Y-%m-%d %H:%M:%S", &tm))
                        {
                            strftime(formatted_date_envoi, sizeof(formatted_date_envoi), "%H:%M:%S le %d/%m/%Y", &tm);
                        }
                        else
                        {
                            strcpy(formatted_date_envoi, "N/A");
                        }

                        if (strptime(date_modification, "%Y-%m-%d %H:%M:%S", &tm))
                        {
                            strftime(formatted_date_modification, sizeof(formatted_date_modification), "%H:%M:%S le %d/%m/%Y", &tm);
                        }
                        else
                        {
                            strcpy(formatted_date_modification, "N/A");
                        }

                        if (strptime(date_lecture, "%Y-%m-%d %H:%M:%S", &tm))
                        {
                            strftime(formatted_date_lecture, sizeof(formatted_date_lecture), "%H:%M:%S le %d/%m/%Y", &tm);
                        }
                        else
                        {
                            strcpy(formatted_date_lecture, "N/A");
                        }

                        char *modifie = (date_modification && strlen(date_modification) > 0) ? "oui" : "non";
                        char *lu = (date_lecture && strlen(date_lecture) > 0) ? "oui" : "non";

                        if (strcmp(id_envoyeur, id_compte_client) != 0 && strcmp(id_receveur, id_compte_client) != 0)
                        {
                            logs("Le client n'a pas accès au message.", id_compte_client, client_ip, verbose);
                            send_answer(cnx, params, "403", id_compte_client, client_ip, verbose);
                        }
                        else
                        {
                            if (strcmp(id_receveur, id_compte_client) == 0 && strcmp(lu, "non") == 0)
                            {
                                char update_query[256];
                                snprintf(update_query, sizeof(update_query), "UPDATE sae_db._message SET date_lecture = NOW() WHERE id = '%s';", id_message);
                                execute(conn, update_query);
                            }
                            char *info_message = malloc(strlen(id_envoyeur) + strlen(id_receveur) + strlen(message) + strlen(modifie) + strlen(date_modification) + strlen(lu) + strlen(date_lecture) + strlen(date_envoi) + 200);

                            if (info_message)
                            {
                                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                                snprintf(info_message, strlen(id_envoyeur) + strlen(id_receveur) + strlen(message) + strlen(modifie) + strlen(date_modification) + strlen(lu) + strlen(date_lecture) + strlen(date_envoi) + 200,
                                         "\"%s\", %s %s à %s.\nÉtat: %s%s, Statut: %s%s.\n",
                                         message,
                                         (strcmp(id_compte_client, id_receveur) == 0) ? "Envoyé par" : "Reçu par",
                                         (strcmp(id_compte_client, id_receveur) == 0) ? id_envoyeur : id_receveur,
                                         formatted_date_envoi,
                                         (strcmp(modifie, "oui") == 0) ? "Modifié à " : "Original",
                                         (strcmp(modifie, "oui") == 0) ? formatted_date_modification : "",
                                         (strcmp(lu, "oui") == 0) ? "Lu à " : "Envoyé",
                                         (strcmp(lu, "oui") == 0) ? formatted_date_lecture : "");
                                write(cnx, info_message, strlen(info_message));
                                logs("Informations du message envoyées.", id_compte_client, client_ip, verbose);
                                free(info_message);
                            }
                            else
                            {
                                logs("Échec de l'allocation de mémoire pour les informations du message.", id_compte_client, client_ip, verbose);
                                perror("Failed to allocate memory for info message");
                                send_answer(cnx, params, "500", id_compte_client, client_ip, verbose);
                            }
                        }
                    }
                }
                else
                {
                    logs("Le client n'est pas connecté en tant que membre ou professionnel.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "401", id_compte_client, client_ip, verbose);
                }
            }
            else if (strncmp(trimmed_buffer, "/modifie ", 9) == 0)
            {
                if (strcmp(trimmed_buffer, "/modifie -h") == 0 || strcmp(trimmed_buffer, "/modifie --help") == 0)
                {
                    logs("Commande d'aide /modifie", id_compte_client, client_ip, verbose);
                    write(cnx, "Usage: /modifie {id_message} {nouveau_message}\nRemplace le contenu du message spécifié.\n", 79);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else if (strcmp(id_compte_client, "") != 0 && strcmp(id_compte_client, "admin") != 0)
                {
                    char *id_message = strtok(trimmed_buffer + 9, " ");
                    char *nouveau_message = strtok(NULL, "");

                    char *taille_maxi = get_param(params, "max_message_size");
                    if (strlen(nouveau_message) > atoi(taille_maxi))
                    {
                        logs("Le message est trop long.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "413", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    if (message_existe(conn, id_message) == 0)
                    {
                        logs("Le message n'existe pas.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    if (client_est_envoyeur(conn, id_compte_client, id_message) == 0)
                    {
                        logs("Le client n'est pas l'envoyeur du message.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "403", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    char query[256];
                    snprintf(query, sizeof(query), "UPDATE sae_db._message SET message = '%s', date_modification = NOW() WHERE id = '%s';", nouveau_message, id_message);
                    execute(conn, query);
                    logs("Message modifié.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else
                {
                    logs("Le client n'est pas connecté ou est un admin.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "401", id_compte_client, client_ip, verbose);
                }
            }
            else if (strncmp(trimmed_buffer, "/supprime ", 10) == 0)
            {
                if (strcmp(trimmed_buffer, "/supprime -h") == 0 || strcmp(trimmed_buffer, "/supprime --help") == 0)
                {
                    logs("Commande d'aide /supprime", id_compte_client, client_ip, verbose);
                    write(cnx, "Usage: /supprime {id_message}\nSupprime le message spécifié.\n", 63);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else
                {
                    // Vérifier si le message existe
                    char *id_mesage = trimmed_buffer + 10;
                    char query[256];
                    PGresult *res = NULL;

                    if (is_positive_integer(id_mesage))
                    {
                        snprintf(query, sizeof(query), "SELECT id_envoyeur FROM sae_db._message WHERE id = '%s' AND date_suppression IS NULL;", id_mesage);
                        res = execute(conn, query);
                    }

                    // Le message existe-t-il ?
                    if (PQntuples(res) > 0)
                    {
                        // Vérifier si ce message est à nous
                        if (atoi(PQgetvalue(res, 0, 0)) == atoi(id_compte_client))
                        {
                            // Mettre la date de suppression à la date actuelle (si pas déjà supprimé)
                            char *id_mesage = trimmed_buffer + 10;
                            char query[256];

                            snprintf(query, sizeof(query), "UPDATE sae_db._message SET date_suppression = NOW() WHERE id = '%d' AND date_suppression IS NULL;", atoi(id_mesage));
                            execute(conn, query);
                            logs("Message supprimé.", id_compte_client, client_ip, verbose);
                            send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                        }
                        else
                        {
                            logs("Le client n'est pas l'envoyeur du message.", id_compte_client, client_ip, verbose);
                            send_answer(cnx, params, "403", id_compte_client, client_ip, verbose);
                        }
                    }
                    else
                    {
                        logs("Le message n'existe pas.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                    }
                }
            }

            // ##########################
            // # BLOCAGE & BANNISSEMENT #
            // ##########################
            else if (strncmp(trimmed_buffer, "/bloque ", 8) == 0)
            {
                if (strcmp(trimmed_buffer, "/bloque -h") == 0 || strcmp(trimmed_buffer, "/bloque --help") == 0)
                {
                    logs("Commande d'aide /bloque", id_compte_client, client_ip, verbose);
                    write(cnx, "Usage: /bloque {id_client}\nBloque le client spécifié.\n", 57);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else if (strcmp(id_compte_client, "admin") == 0 || client_est_pro(conn, id_compte_client) > 0)
                {
                    char *id_client = trimmed_buffer + 8;
                    if (client_existe(conn, id_client) == 0)
                    {
                        logs("Le client à bloquer n'existe pas.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    if (client_est_bloque(conn, id_client, id_compte_client) > 0)
                    {
                        logs("Le client est déjà bloqué.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "409", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    char query[2048];
                    if (strcmp(id_compte_client, "admin") == 0)
                    {
                        snprintf(query, sizeof(query), "INSERT INTO sae_db._blocage (id_bloque, id_bloqueur) VALUES ('%s', '-1');", id_client);
                    }
                    else
                    {
                        snprintf(query, sizeof(query), "INSERT INTO sae_db._blocage (id_bloque, id_bloqueur) VALUES ('%s', '%s');", id_client, id_compte_client);
                    }
                    execute(conn, query);

                    logs("Client bloqué.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else
                {
                    logs("Le client n'est pas connecté en tant que professionnel ou admin.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "401", id_compte_client, client_ip, verbose);
                }
            }
            else if (strncmp(trimmed_buffer, "/debloque ", 10) == 0)
            {
                if (strcmp(trimmed_buffer, "/debloque -h") == 0 || strcmp(trimmed_buffer, "/debloque --help") == 0)
                {
                    logs("Commande d'aide /debloque", id_compte_client, client_ip, verbose);
                    write(cnx, "Usage: /debloque {id_client}\nLève le blocage d'un client spécifié.\n", 57);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else if (strcmp(id_compte_client, "admin") == 0 || client_est_pro(conn, id_compte_client) > 0)
                {
                    char *id_client = trimmed_buffer + 10;
                    if (client_existe(conn, id_client) == 0)
                    {
                        logs("Le client à débloquer n'existe pas.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    if (client_est_bloque(conn, id_client, id_compte_client) == 0)
                    {
                        logs("Le client n'est pas bloqué.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "409", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    char query[2048];
                    char id_blocage[3];

                    if (strcmp(id_compte_client, "admin") == 0)
                    {
                        snprintf(query, sizeof(query), "SELECT id FROM sae_db._blocage WHERE id_bloque = '%s' AND id_bloqueur = '-1' ORDER BY date_blocage DESC LIMIT 1;", id_client);
                    }
                    else
                    {
                        snprintf(query, sizeof(query), "SELECT id FROM sae_db._blocage WHERE id_bloque = '%s' AND id_bloqueur = '%s' ORDER BY date_blocage DESC LIMIT 1;", id_client, id_compte_client);
                    }
                    PGresult *res = execute(conn, query);
                    if (PQntuples(res) > 0)
                    {
                        strcpy(id_blocage, PQgetvalue(res, 0, 0));
                    }
                    PQclear(res);

                    if (strcmp(id_compte_client, "admin") == 0)
                    { // mets à jour la dernière entrée de blocage
                        snprintf(query, sizeof(query), "UPDATE sae_db._blocage SET date_deblocage = NOW() WHERE id = '%s';", id_blocage);
                    }
                    else
                    {
                        snprintf(query, sizeof(query), "UPDATE sae_db._blocage SET date_deblocage = NOW() WHERE id = '%s';", id_blocage);
                    }
                    execute(conn, query);

                    logs("Client débloqué.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else
                {
                    logs("Le client n'est pas connecté en tant que professionnel ou admin.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "401", id_compte_client, client_ip, verbose);
                }
            }
            else if (strncmp(trimmed_buffer, "/ban ", 5) == 0)
            {
                if (strcmp(trimmed_buffer, "/ban -h") == 0 || strcmp(trimmed_buffer, "/ban --help") == 0)
                {
                    logs("Commande d'aide /ban", id_compte_client, client_ip, verbose);
                    write(cnx, "Usage: /ban {id_client}\nBannit le client spécifié.\n", 54);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else if (strcmp(id_compte_client, "admin") == 0)
                {
                    char *id_client = trimmed_buffer + 5;
                    if (client_existe(conn, id_client) == 0)
                    {
                        logs("Le client à bannir n'existe pas.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    if (client_est_banni(conn, id_client) == 1)
                    {
                        logs("Le client est déjà banni.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "409", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    char query[256];
                    snprintf(query, sizeof(query), "INSERT INTO sae_db._bannissement (id_banni) VALUES ('%s');", id_client);
                    execute(conn, query);
                    logs("Client banni.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else
                {
                    logs("Le client n'est pas connecté en tant qu'admin.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "401", id_compte_client, client_ip, verbose);
                }
            }
            else if (strncmp(trimmed_buffer, "/deban ", 7) == 0)
            {
                if (strcmp(trimmed_buffer, "/deban -h") == 0 || strcmp(trimmed_buffer, "/deban --help") == 0)
                {
                    logs("Commande d'aide /deban", id_compte_client, client_ip, verbose);
                    write(cnx, "Usage: /deban {id_client}\nLève le bannissement du client spécifié.\n", 71);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else if (strcmp(id_compte_client, "admin") == 0)
                {
                    char *id_client = trimmed_buffer + 7;
                    if (client_existe(conn, id_client) == 0)
                    {
                        logs("Le client à débannir n'existe pas.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    if (client_est_banni(conn, id_client) == 0)
                    {
                        logs("Le client n'est pas banni.", id_compte_client, client_ip, verbose);
                        send_answer(cnx, params, "409", id_compte_client, client_ip, verbose);
                        continue;
                    }

                    char query[256];
                    snprintf(query, sizeof(query), "UPDATE sae_db._bannissement SET date_debannissement = NOW() WHERE id_banni = '%s' AND date_debannissement IS NULL;", id_client);
                    execute(conn, query);

                    logs("Client débanni.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else
                {
                    logs("Le client n'est pas connecté en tant qu'admin.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "401", id_compte_client, client_ip, verbose);
                }
            }

            // ###############
            // # PARAMETRAGE #
            // ###############
            else if (strncmp(trimmed_buffer, "/sync", 5) == 0)
            {
                if (strcmp(trimmed_buffer, "/sync -h") == 0 || strcmp(trimmed_buffer, "/sync --help") == 0)
                {
                    logs("Commande d'aide /sync", id_compte_client, client_ip, verbose);
                    write(cnx, "Usage: /sync\nRecharge le fichier de paramétrage.\n", 51);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else if (strcmp(id_compte_client, "admin") == 0)
                {
                    logs("Rechargement du fichier de paramétrage.", id_compte_client, client_ip, verbose);
                    read_param_file(params, param_file);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else
                {
                    logs("Le client n'est pas connecté en tant qu'admin.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "401", id_compte_client, client_ip, verbose);
                }
            }
            else if (strncmp(trimmed_buffer, "/logs", 5) == 0)
            {
                if (strcmp(trimmed_buffer, "/logs -h") == 0 || strcmp(trimmed_buffer, "/logs --help") == 0)
                {
                    logs("Commande d'aide /logs", id_compte_client, client_ip, verbose);
                    write(cnx, "Usage: /logs {?nb_logs=50}\nAffiche les logs.\n", 46);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else if (strcmp(id_compte_client, "admin") == 0)
                {
                    char *nb_logs = strtok(trimmed_buffer + 6, " ");
                    if (nb_logs == NULL)
                    {
                        nb_logs = "50";
                    }

                    char *log_file = get_param(params, "fichier_logs");
                    FILE *file = fopen(log_file, "r");
                    if (!file)
                    {
                        logs("Échec de l'ouverture du fichier de logs.", id_compte_client, client_ip, verbose);
                        perror("fopen failed");
                        send_answer(cnx, params, "500", id_compte_client, client_ip, verbose);
                    }
                    else
                    {
                        fseek(file, 0, SEEK_END);
                        long file_size = ftell(file);
                        fseek(file, 0, SEEK_SET);

                        char *file_content = malloc(file_size + 1);
                        fread(file_content, 1, file_size, file);
                        file_content[file_size] = '\0';

                        fclose(file);

                        char *line = strtok(file_content, "\n");
                        int line_count = 0;
                        while (line)
                        {
                            line_count++;
                            line = strtok(NULL, "\n");
                        }

                        free(file_content);

                        file = fopen(log_file, "r");
                        if (!file)
                        {
                            logs("Échec de l'ouverture du fichier de logs.", id_compte_client, client_ip, verbose);
                            perror("fopen failed");
                            send_answer(cnx, params, "500", id_compte_client, client_ip, verbose);
                        }
                        else
                        {
                            send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                            file_content = malloc(file_size + 1);
                            fread(file_content, 1, file_size, file);
                            file_content[file_size] = '\0';

                            fclose(file);

                            line = strtok(file_content, "\n");
                            int start_line = line_count - atoi(nb_logs);
                            if (start_line < 0)
                            {
                                start_line = 0;
                            }

                            line_count = 0;
                            while (line)
                            {
                                if (line_count >= start_line)
                                {
                                    write(cnx, line, strlen(line));
                                    write(cnx, "\n", 1);
                                }
                                line_count++;
                                line = strtok(NULL, "\n");
                            }

                            logs("Logs envoyés.", id_compte_client, client_ip, verbose);
                            free(file_content);
                        }
                    }
                }
                else
                {
                    logs("Le client n'est pas connecté en tant qu'admin.", id_compte_client, client_ip, verbose);
                    send_answer(cnx, params, "401", id_compte_client, client_ip, verbose);
                }
            }
            else
            {
                logs("Commande invalide.", id_compte_client, client_ip, verbose);
                send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
            }

        } while (len > 0);

        close(cnx);
    }
    close(sock);

    return 0;
}
