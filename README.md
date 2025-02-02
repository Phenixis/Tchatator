# Tchatator

## BUGS

- [x] Aucune file d'attente n'est gérée.

## À FAIRE

Pour les requêtes à la base de données, lors de la connexion, l'id du compte client est enregistré dans id_compte_client.

- [x] Connexion -> 200/OK
- [x] Connexion -> 401/UNAUTHORIZED
- [x] Déconnexion -> 200/OK
- [x] Envoi de message -> 200/OK
- [x] Envoi de message -> 413/PAYLOAD TOO LARGE
- [x] Envoi de message -> 404/NOT FOUND
- [x] Liste des messages non lus -> 200/OK
- [x] Liste des messages non lus -> 204/NO CONTENT
- [x] Liste des messages non lus -> 416/RANGE NOT SATISFIABLE
- [x] Historique des messages -> 200/OK
- [x] Historique des messages -> 204/NO CONTENT
- [x] Historique des messages -> 404/NOT FOUND
- [x] Historique des messages -> 416/RANGE NOT SATISFIABLE
- [x] Informations sur un message -> 200/OK
- [x] Informations sur un message -> 404/NOT FOUND
- [x] Informations sur un message -> 403/FORBIDDEN
- [x] Modification d'un message -> 200/OK
- [x] Modification d'un message -> 404/NOT FOUND
- [x] Modification d'un message -> 403/FORBIDDEN
- [x] Suppression d'un message -> 200/OK
- [x] Suppression d'un message -> 404/NOT FOUND
- [x] Suppression d'un message -> 403/FORBIDDEN
- [x] Blocage d'un client -> 200/OK
- [x] Blocage d'un client -> 409/CONFLICT
- [x] Blocage d'un client -> 404/NOT FOUND
- [x] Bannissement d'un client -> 200/OK
- [x] Bannissement d'un client -> 409/CONFLICT
- [x] Bannissement d'un client -> 404/NOT FOUND
- [x] Levage d'un bannissement -> 200/OK
- [x] Levage d'un bannissement -> 404/NOT FOUND
- [x] Synchronisation des paramètres -> 200/OK
- [x] Récupération des logs -> 200/OK