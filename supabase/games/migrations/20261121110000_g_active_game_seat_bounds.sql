-- Keep active game seats aligned with the games actually registered in ANIX PRO.
-- Talk-room members and spectators remain separate and have no product-level cap.
-- The older bounds over-seated two-player games (Carrom / Quiz) and allowed more
-- Spy & Mafia players than its core, UI palette, and active registry support.
create or replace function public.nx_known_game(p_game text)
returns boolean
language sql
immutable
as $$
  select p_game = any(array[
    'finger_soccer', 'carrom', 'billiards', 'backgammon', 'ludo',
    'snakes_ladders', 'uno', 'quiz', 'mafia', 'kart_racing',
    'obstacle_race', 'bomb_arena', 'pool8', 'mini_golf',
    'basketball_battle', 'spy_mafia'
  ]::text[])
$$;

create or replace function public.nx_game_bounds(
  p_game text,
  out min_seats integer,
  out max_seats integer
)
returns record
language sql
immutable
as $$
  select
    case p_game
      when 'mafia' then 6
      when 'spy_mafia' then 4
      else 2
    end,
    case p_game
      when 'ludo' then 4
      when 'snakes_ladders' then 4
      when 'uno' then 4
      when 'mafia' then 12
      when 'kart_racing' then 4
      when 'obstacle_race' then 4
      when 'bomb_arena' then 4
      when 'mini_golf' then 4
      when 'basketball_battle' then 4
      when 'spy_mafia' then 8
      when 'finger_soccer' then 6
      else 2
    end
$$;

create or replace function public._nx_room_seat_limit(p_game text)
returns integer
language sql
immutable
as $$
  select (public.nx_game_bounds(p_game)).max_seats
$$;

-- Reject invalid seat requests instead of silently creating rooms that the game
-- core cannot run (including odd-number Finger Soccer teams).
create or replace function public.nx_create_room(
  p_game text,
  p_seats integer,
  p_private boolean,
  p_quick boolean default false,
  p_settings jsonb default '{}'::jsonb
)
returns jsonb
language plpgsql
security definer
set search_path to 'public'
as $$
declare
  me uuid := auth.uid();
  b record;
  rid uuid;
  c text;
begin
  if me is null then raise exception 'not signed in'; end if;
  if not public.nx_known_game(p_game) then raise exception 'unknown game'; end if;
  select * into b from public.nx_game_bounds(p_game);
  if p_seats is null or p_seats < b.min_seats or p_seats > b.max_seats then
    raise exception 'invalid seat count for % (allowed %..%)', p_game, b.min_seats, b.max_seats;
  end if;
  if p_game = 'finger_soccer' and p_seats % 2 <> 0 then
    raise exception 'finger soccer requires an even number of active players';
  end if;
  perform public._nx_leave_all(me);
  c := public.nx_gen_code();
  insert into public.nx_rooms(code, game, host, seats, is_private, quick, settings)
  values (c, p_game, me, p_seats, coalesce(p_private, false), coalesce(p_quick, false), coalesce(p_settings, '{}'::jsonb))
  returning id into rid;
  insert into public.nx_room_players(room_id, user_id, seat, ready)
  values (rid, me, 0, true);
  return jsonb_build_object('room_id', rid, 'code', c, 'seats', p_seats);
end
$$;

create or replace function public.nx_quick(p_game text, p_seats integer)
returns jsonb
language plpgsql
security definer
set search_path to 'public'
as $$
declare
  me uuid := auth.uid();
  rid uuid;
  res jsonb;
  n int;
  r public.nx_rooms;
  mine public.nx_rooms;
  b record;
begin
  if me is null then raise exception 'not signed in'; end if;
  if not public.nx_known_game(p_game) then raise exception 'unknown game'; end if;
  select * into b from public.nx_game_bounds(p_game);
  if p_seats is null or p_seats < b.min_seats or p_seats > b.max_seats then
    raise exception 'invalid seat count for % (allowed %..%)', p_game, b.min_seats, b.max_seats;
  end if;
  if p_game = 'finger_soccer' and p_seats % 2 <> 0 then
    raise exception 'finger soccer requires an even number of active players';
  end if;

  delete from public.nx_room_players rp using public.nx_rooms x
   where x.id = rp.room_id and x.quick and x.status = 'lobby' and rp.user_id <> me
     and rp.last_seen < now() - interval '45 seconds';
  delete from public.nx_room_members m using public.nx_rooms x
   where x.id = m.room_id and x.quick and x.status = 'lobby'
     and not exists(select 1 from public.nx_room_players rp where rp.room_id = x.id and rp.user_id = m.user_id);
  update public.nx_rooms x set status = 'closed'
   where x.status = 'lobby' and x.quick
     and (x.updated_at < now() - interval '10 minutes'
       or not exists(select 1 from public.nx_room_players rp where rp.room_id = x.id));

  select x.* into mine from public.nx_rooms x
   join public.nx_room_players rp on rp.room_id = x.id and rp.user_id = me
   where x.quick and x.status = 'lobby' and x.game = p_game and x.seats = p_seats limit 1;
  if mine.id is not null then
    update public.nx_room_players set last_seen = now() where room_id = mine.id and user_id = me;
    select count(*) into n from public.nx_room_players where room_id = mine.id;
    if n >= mine.seats then
      return jsonb_build_object('room_id', mine.id, 'code', mine.code, 'rejoined', true)
        || public.nx_start_match_quick(mine.id);
    end if;
  end if;

  select x.id into rid from public.nx_rooms x
   where x.game = p_game and x.status = 'lobby' and x.quick and not x.is_private and x.seats = p_seats
     and (mine.id is null or x.id <> mine.id)
     and not exists(select 1 from public.nx_room_players rp where rp.room_id = x.id and rp.user_id = me)
     and (select count(*) from public.nx_room_players rp where rp.room_id = x.id) < x.seats
   order by x.created_at limit 1 for update skip locked;

  if rid is null then
    if mine.id is not null then
      return jsonb_build_object('room_id', mine.id, 'code', mine.code, 'seats', mine.seats, 'rejoined', true);
    end if;
    res := public.nx_create_room(p_game, p_seats, false, true, '{}'::jsonb);
    return res || jsonb_build_object('created', true);
  end if;

  perform public._nx_leave_all(me);
  res := public.nx_join_room(null, rid);
  update public.nx_room_players set last_seen = now() where room_id = rid and user_id = me;
  select * into r from public.nx_rooms where id = rid;
  select count(*) into n from public.nx_room_players where room_id = rid;
  if n >= r.seats then res := res || public.nx_start_match_quick(rid); end if;
  return res;
end
$$;

-- Old quick rooms may contain an invalid seat count; validate before creating a match.
create or replace function public.nx_start_match_quick(p_room uuid)
returns jsonb
language plpgsql
security definer
set search_path to 'public'
as $$
declare
  r public.nx_rooms;
  b record;
  ids uuid[];
  nm text[];
  mid uuid;
  sd bigint;
  n int;
begin
  select * into r from public.nx_rooms where id = p_room for update;
  if r.id is null then raise exception 'room not found'; end if;
  if r.status <> 'lobby' then return jsonb_build_object('match_id', r.match_id); end if;
  select * into b from public.nx_game_bounds(r.game);
  select array_agg(rp.user_id order by rp.seat),
         array_agg(coalesce(p.username, 'Pilot') order by rp.seat), count(*)
    into ids, nm, n
    from public.nx_room_players rp
    left join public.profiles p on p.id = rp.user_id
   where rp.room_id = p_room;
  if n < b.min_seats or n > b.max_seats or n <> r.seats then
    raise exception 'invalid active player count';
  end if;
  if r.game = 'finger_soccer' and n % 2 <> 0 then
    raise exception 'finger soccer requires an even number of active players';
  end if;
  sd := floor(random() * 2147483646)::bigint + 1;
  insert into public.nx_matches(room_id, game, players, names, seed, settings)
  values (p_room, r.game, ids, nm, sd, r.settings)
  returning id into mid;
  update public.nx_rooms set status = 'playing', match_id = mid, updated_at = now() where id = p_room;
  return jsonb_build_object('match_id', mid, 'seed', sd);
end
$$;

create or replace function public.nx_start_match(p_room uuid)
returns jsonb
language plpgsql
security definer
set search_path to 'public'
as $$
declare
  me uuid := auth.uid();
  r public.nx_rooms;
  b record;
  ids uuid[];
  nm text[];
  mid uuid;
  sd bigint;
  n int;
begin
  select * into r from public.nx_rooms where id = p_room for update;
  if r.id is null then raise exception 'room not found'; end if;
  if r.host <> me and not r.quick then raise exception 'host only'; end if;
  if r.status = 'playing' and r.match_id is not null then
    return jsonb_build_object('match_id', r.match_id, 'already', true);
  end if;
  select * into b from public.nx_game_bounds(r.game);
  select array_agg(rp.user_id order by rp.seat),
         array_agg(coalesce(p.username, 'Pilot') order by rp.seat), count(*)
    into ids, nm, n
    from public.nx_room_players rp
    left join public.profiles p on p.id = rp.user_id
   where rp.room_id = p_room;
  if n < b.min_seats or n > b.max_seats then raise exception 'invalid active player count'; end if;
  if n > r.seats then raise exception 'active player count exceeds room seats'; end if;
  if r.game = 'finger_soccer' and n % 2 <> 0 then
    raise exception 'finger soccer requires an even number of active players';
  end if;
  sd := floor(random() * 2147483646)::bigint + 1;
  insert into public.nx_matches(room_id, game, players, names, seed, settings)
  values (p_room, r.game, ids, nm, sd, r.settings)
  returning id into mid;
  update public.nx_rooms set status = 'playing', match_id = mid, updated_at = now() where id = p_room;
  return jsonb_build_object('match_id', mid, 'seed', sd);
end
$$;

create or replace function public.nx_update_room(
  p_room uuid,
  p_seats integer,
  p_settings jsonb
)
returns void
language plpgsql
security definer
set search_path to 'public'
as $$
declare
  r public.nx_rooms;
  b record;
  requested int;
  active_count int;
begin
  select * into r from public.nx_rooms where id = p_room for update;
  if r.id is null then raise exception 'room not found'; end if;
  if r.host <> auth.uid() then raise exception 'host only'; end if;
  if r.status <> 'lobby' then raise exception 'already started'; end if;
  select * into b from public.nx_game_bounds(r.game);
  select count(*) into active_count from public.nx_room_players where room_id = p_room;
  requested := coalesce(p_seats, r.seats);
  requested := greatest(requested, active_count, b.min_seats);
  if requested > b.max_seats then
    raise exception 'room capacity exceeds active-seat limit for %', r.game;
  end if;
  if r.game = 'finger_soccer' and requested % 2 <> 0 then
    raise exception 'finger soccer room capacity must be even';
  end if;
  update public.nx_rooms
     set seats = requested,
         settings = coalesce(p_settings, settings),
         updated_at = now()
   where id = p_room;
end
$$;

notify pgrst, 'reload schema';
