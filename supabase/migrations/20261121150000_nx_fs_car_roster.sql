-- Four new car-piece teams from the owner's 2026-11-21 art sheet.
-- The client falls back to the original luxury-team price (500 gems), but
-- purchases are authorized and priced solely by nx_catalog / nx_buy_with_gems.
-- Append to the catalog in a new migration: do not re-run the old seed on a
-- hosted database or overwrite prices that an admin has intentionally tuned.
insert into public.nx_catalog(item_id, price, currency, kind, name, active) values
  ('fs_team:car14', 500, 'gems', 'team', 'Audi', true),
  ('fs_team:car15', 500, 'gems', 'team', 'Volkswagen', true),
  ('fs_team:car16', 500, 'gems', 'team', 'Toyota', true),
  ('fs_team:car17', 500, 'gems', 'team', 'Nissan', true)
on conflict (item_id) do nothing;
