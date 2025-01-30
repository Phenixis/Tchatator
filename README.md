# Tchatator

## BUGS

- [ ] Aucune file d'attente n'est gérée.

## À FAIRE

Pour les requêtes à la base de données, lors de la connexion, l'id du compte client est enregistré dans id_compte_client.

- [x] Connexion -> 200/OK
- [x] Connexion -> 401/UNAUTHORIZED
- [x] Déconnexion -> 200/OK
- [ ] Envoi de message -> 200/OK
- [ ] Envoi de message -> 200/OK
- [ ] Envoi de message -> 413/PAYLOAD TOO LARGE
- [ ] Envoi de message -> 404/NOT FOUND
- [ ] Liste des messages non lus -> 200/OK
- [ ] Liste des messages non lus -> 204/NO CONTENT
- [ ] Liste des messages non lus -> 416/RANGE NOT SATISFIABLE
- [ ] Historique des messages -> 200/OK
- [ ] Historique des messages -> 204/NO CONTENT
- [ ] Historique des messages -> 404/NOT FOUND
- [ ] Historique des messages -> 416/RANGE NOT SATISFIABLE
- [ ] Informations sur un message -> 200/OK
- [ ] Informations sur un message -> 404/NOT FOUND
- [ ] Informations sur un message -> 403/FORBIDDEN
- [ ] Modification d'un message -> 200/OK
- [ ] Modification d'un message -> 404/NOT FOUND
- [ ] Modification d'un message -> 403/FORBIDDEN
- [ ] Suppression d'un message -> 200/OK
- [ ] Suppression d'un message -> 404/NOT FOUND
- [ ] Suppression d'un message -> 403/FORBIDDEN
- [ ] Blocage d'un client -> 200/OK
- [ ] Blocage d'un client -> 409/CONFLICT
- [ ] Blocage d'un client -> 404/NOT FOUND
- [x] Bannissement d'un client -> 200/OK
- [x] Bannissement d'un client -> 409/CONFLICT
- [x] Bannissement d'un client -> 404/NOT FOUND
- [x] Levage d'un bannissement -> 200/OK
- [x] Levage d'un bannissement -> 404/NOT FOUND
- [x] Synchronisation des paramètres -> 200/OK
- [x] Récupération des logs -> 200/OK