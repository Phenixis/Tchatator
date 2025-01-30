#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>

#define FIFO_IN "to_tchatator"
#define FIFO_OUT "from_tchatator"

int count = 0;
int fd_in;
int fd_out;

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
    while (len > 0 && (trimmed_str[0] == '\n' || trimmed_str[0] == '\r' || isspace(trimmed_str[0])))
    {
        trimmed_str++;
        len--;
    }
    return trimmed_str;
}

int test(char *commande, char *reponse_attendue)
{
    count++;
    char *test_data = malloc(100);
    sprintf(test_data, "%s\n", commande);
    write(fd_in, test_data, strlen(test_data));

    char buffer[100];
    int bytes_read = read(fd_out, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';

        char *trimmed_buffer = trim_newline(buffer);
        if (strcmp(reponse_attendue, trimmed_buffer) != 0)
        {
            printf("Test %d échoué :\n", count);
            printf("\t%s\n", commande);
            printf("\tréponse : [%s]\n", trimmed_buffer);
            printf("\tattendu : [%s]\n", reponse_attendue);
        }
        else
        {
            printf("test réussi\n");
        }
        free(trimmed_buffer);
    }
    return 0;
}

int main()
{
    fd_in = open(FIFO_IN, O_WRONLY);
    fd_out = open(FIFO_OUT, O_RDONLY | O_ASYNC);

    // requêtes interdites sans connexion
    // test("/message ", "401/UNAUTHORIZED");
    // test("/liste ", "401/UNAUTHORIZED");
    // test("/conversation ", "401/UNAUTHORIZED");
    // test("/info ", "401/UNAUTHORIZED");
    // test("/modifie ", "401/UNAUTHORIZED");
    // test("/supprime ", "401/UNAUTHORIZED");
    // test("/bloque ", "401/UNAUTHORIZED");
    // test("/debloque ", "401/UNAUTHORIZED");
    // test("/ban ", "401/UNAUTHORIZED");
    // test("/deban ", "401/UNAUTHORIZED");
    // test("/logs ", "401/UNAUTHORIZED");
    // test("/sync ", "401/UNAUTHORIZED");

    // requêtes administrateur
    test("/connexion tchatator_FNOC_TO_THE_TOP", "200/OK");
    test("/ban 1", "200/OK");
    test("/ban 1", "409/CONFLICT");
    test("/ban 7987987", "404/NOT FOUND");
    test("/deban 1", "200/OK");
    test("/deban 1", "409/CONFLICT");
    test("/deban 7987987", "404/NOT FOUND");
    test("/sync", "200/OK");
    // test("/logs", "200/OK"); // On ne peut pas tester cette commande car elle dépend du contenu du fichier de logs
    test("/deconnexion", "200/OK");

    close(fd_in);
    close(fd_out);

    return 0;
}
