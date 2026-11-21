-- Run on STAGING after games-project migrations (psql -v ON_ERROR_STOP=1).
-- Read-only structure check; exercising crafted moves needs isolated test users.
begin;
do $$
begin
  if not exists (select 1 from pg_trigger where tgrelid = 'public.nx_match_moves'::regclass
     and tgname = 'nx_fs_no_custom_match_picks' and tgenabled <> 'D') then
    raise exception 'online setup/tpick custom guard is missing';
  end if;
  if not exists (select 1 from pg_trigger where tgrelid = 'public.nx_room_game_players'::regclass
     and tgname = 'nx_fs_no_custom_room_prefs' and tgenabled <> 'D') then
    raise exception 'online room-prefs custom guard is missing';
  end if;
end $$;
rollback;
