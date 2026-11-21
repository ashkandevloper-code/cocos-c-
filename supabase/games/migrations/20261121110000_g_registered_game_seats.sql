-- NEXORA PRO · registered games and ACTIVE match seats, not room membership.
-- Forward-only: no rooms, matches or members are removed. Apply on staging first.
-- The shipped Flutter registry has 16 executable games. The other 13 arcade IDs
-- currently have only metadata/skin.json; in particular stickman_arena,
-- ludo_battle and chess_blitz must not create matches without a game entry.
create or replace function public.nx_known_game(p_game text) returns boolean
language sql immutable set search_path = public as $$
  select coalesce(p_game in (
    'finger_soccer', 'carrom', 'billiards', 'backgammon', 'ludo',
    'snakes_ladders', 'uno', 'quiz', 'mafia', 'kart_racing',
    'obstacle_race', 'bomb_arena', 'pool8', 'mini_golf',
    'basketball_battle', 'spy_mafia'
  ), false)
$$;

-- These limits apply ONLY to active players. Social/talk room members and
-- spectators retain NULL capacity (infrastructure-managed, no product cap).
-- Finger Soccer alone supports 2/4/6 players in team rooms; the talk-room
-- match-start RPC already rejects odd teams.
create or replace function public.nx_game_bounds(p_game text, out min_seats integer, out max_seats integer)
returns record language sql immutable set search_path = public as $$
  select case p_game
           when 'mafia' then 6
           when 'spy_mafia' then 4
           when 'finger_soccer' then 2
           when 'carrom' then 2
           when 'billiards' then 2
           when 'backgammon' then 2
           when 'ludo' then 2
           when 'snakes_ladders' then 2
           when 'uno' then 2
           when 'quiz' then 2
           when 'kart_racing' then 2
           when 'obstacle_race' then 2
           when 'bomb_arena' then 2
           when 'pool8' then 2
           when 'mini_golf' then 2
           when 'basketball_battle' then 2
         end,
         case p_game
           when 'finger_soccer' then 6
           when 'mafia' then 12
           when 'spy_mafia' then 8
           when 'ludo' then 4
           when 'snakes_ladders' then 4
           when 'uno' then 4
           when 'kart_racing' then 4
           when 'obstacle_race' then 4
           when 'bomb_arena' then 4
           when 'mini_golf' then 4
           when 'basketball_battle' then 4
           when 'carrom' then 2
           when 'billiards' then 2
           when 'backgammon' then 2
           when 'quiz' then 2
           when 'pool8' then 2
         end
$$;

revoke all on function public.nx_known_game(text), public.nx_game_bounds(text) from public, anon;
grant execute on function public.nx_known_game(text), public.nx_game_bounds(text) to authenticated, service_role;
