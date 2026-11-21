-- Execute after all games migrations on staging: psql "$GAMES_DATABASE_URL" -v ON_ERROR_STOP=1 -f supabase/games/tests/registered_game_seats.sql
-- Read-only assertions; no match/room data created.
begin;
do $$
declare entry record; bounds record; actual int := 0;
begin
  for entry in select * from (values
    ('finger_soccer', 2, 6), ('carrom', 2, 2), ('billiards', 2, 2),
    ('backgammon', 2, 2), ('ludo', 2, 4), ('snakes_ladders', 2, 4),
    ('uno', 2, 4), ('quiz', 2, 2), ('mafia', 6, 12),
    ('kart_racing', 2, 4), ('obstacle_race', 2, 4),
    ('bomb_arena', 2, 4), ('pool8', 2, 2), ('mini_golf', 2, 4),
    ('basketball_battle', 2, 4), ('spy_mafia', 4, 8)
  ) as games(id, minimum, maximum) loop
    select * into bounds from public.nx_game_bounds(entry.id);
    if not public.nx_known_game(entry.id) or bounds.min_seats is distinct from entry.minimum
        or bounds.max_seats is distinct from entry.maximum then
      raise exception 'bounds disagree with executable game: %', entry.id;
    end if;
    actual := actual + 1;
  end loop;
  if actual <> 16 then raise exception 'expected 16 executable games'; end if;
  if public.nx_known_game('stickman_arena') or public.nx_known_game('ludo_battle')
     or public.nx_known_game('chess_blitz') or public.nx_known_game('game_2048')
     or public.nx_known_game(null) then
    raise exception 'non-executable game accepted by multiplayer';
  end if;
  if exists (
    select 1 from information_schema.columns
    where table_schema = 'public' and table_name = 'nx_rooms'
      and column_name in ('speaker_capacity', 'spectator_capacity')
      and (is_nullable <> 'YES' or column_default is not null)
  ) then raise exception 'room member capacity has a fixed default'; end if;
end $$;
rollback;
