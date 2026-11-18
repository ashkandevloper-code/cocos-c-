-- Removes the e2e test accounts (run with the ids printed by social_e2e.py).
delete from auth.users where email like 'nx.e2e.%@nexora-e2e.dev';
